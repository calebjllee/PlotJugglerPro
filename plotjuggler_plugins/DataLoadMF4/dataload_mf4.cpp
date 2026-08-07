#include "dataload_mf4.h"

#include <QApplication>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>

#include <any>

namespace
{

constexpr const char* MF4_LAZY_METADATA_TYPE = "plotjuggler.mf4.lazy_channel";

QString pythonExecutable()
{
  const QByteArray configured = qgetenv("PJ_ASAMMDF_PYTHON");
  if (!configured.isEmpty())
  {
    return QString::fromLocal8Bit(configured);
  }
  return QStringLiteral("python");
}

QString metadataBridgeScript()
{
  return QStringLiteral(R"PY(
import json
import sys

try:
    import numpy as np
    from asammdf import MDF
except Exception as exc:
    print(json.dumps({
        "type": "error",
        "message": "Python package 'asammdf' is required to load MF4/MDF files: %s" % exc
    }), flush=True)
    sys.exit(2)

filename = sys.argv[1]

def make_unique(base, used):
    name = base or "unnamed"
    if name not in used:
        used[name] = 1
        return name
    used[name] += 1
    return "%s[%d]" % (name, used[name])

def is_numeric_dtype(dtype_fmt):
    try:
        return np.dtype(dtype_fmt).kind in "buif"
    except Exception:
        return False

def has_text_conversion(channel):
    conversion = getattr(channel, "conversion", None)
    if conversion is None:
        return False
    for name in dir(conversion):
        if name.startswith("text_") or name.startswith("upper_") or name.startswith("lower_"):
            return True
    referenced = getattr(conversion, "referenced_blocks", None)
    return bool(referenced)

try:
    mdf = MDF(filename)
except Exception as exc:
    print(json.dumps({
        "type": "error",
        "message": "Failed to open MF4/MDF file: %s" % exc
    }), flush=True)
    sys.exit(3)

used_names = {}
count = 0

for group_index, group in enumerate(mdf.groups):
    channel_group = getattr(group, "channel_group", None)
    master_index = getattr(channel_group, "cg_master_index", None)
    samples = int(getattr(channel_group, "cycles_nr", 0) or 0)

    for channel_index, channel in enumerate(group.channels):
        if master_index is not None and channel_index == master_index:
            continue
        if getattr(channel, "channel_type", 0) != 0:
            continue
        if not is_numeric_dtype(getattr(channel, "dtype_fmt", "")) and not has_text_conversion(channel):
            continue

        raw_name = getattr(channel, "name", "") or "unnamed"
        group_name = ""
        source = getattr(channel, "source", None)
        if source is not None:
            group_name = getattr(source, "name", "") or ""

        if group_name and not raw_name.startswith(group_name + "/"):
            base_name = "%s/%s" % (group_name, raw_name)
        else:
            base_name = raw_name

        series_name = make_unique(base_name.replace("\\", "/"), used_names)
        unit = getattr(channel, "unit", "") or ""

        print(json.dumps({
            "type": "series",
            "metadata_type": "plotjuggler.mf4.lazy_channel",
            "name": series_name,
            "file": filename,
            "group": group_index,
            "index": channel_index,
            "unit": unit,
            "samples": samples
        }), flush=True)
        count += 1

print(json.dumps({
    "type": "done",
    "channels": count
}), flush=True)
)PY");
}

bool processMetadataLine(const QByteArray& line, PJ::PlotDataMapRef& destination,
                         QString& error_message, size_t& loaded_channels)
{
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
  {
    error_message =
        QStringLiteral("Invalid response from asammdf metadata bridge: %1")
            .arg(parse_error.errorString());
    return false;
  }

  QJsonObject object = doc.object();
  const QString type = object.value(QStringLiteral("type")).toString();

  if (type == QStringLiteral("error"))
  {
    error_message = object.value(QStringLiteral("message")).toString();
    return false;
  }

  if (type == QStringLiteral("series"))
  {
    const QString name_qt = object.value(QStringLiteral("name")).toString();
    if (name_qt.isEmpty())
    {
      return true;
    }

    const std::string name = name_qt.toStdString();
    const QString unit = object.value(QStringLiteral("unit")).toString();
    const qint64 samples = qint64(object.value(QStringLiteral("samples")).toDouble());

    auto& series = destination.getOrCreateNumeric(name);
    series.setAttribute(PJ::TOOL_TIP,
                        QStringLiteral("MF4 lazy channel\nState: unloaded\nSamples: %1\nUnit: %2")
                            .arg(samples)
                            .arg(unit.isEmpty() ? QStringLiteral("-") : unit));
    series.setAttribute(PJ::ITALIC_FONTS, true);

    QJsonDocument metadata_doc(object);
    destination.getOrCreateUserDefined(name).pushBack(
        { 0.0, std::any(metadata_doc.toJson(QJsonDocument::Compact).toStdString()) });

    loaded_channels++;
  }

  return true;
}

}  // namespace

const std::vector<const char*>& DataLoadMF4::compatibleFileExtensions() const
{
  static std::vector<const char*> extensions = { "mf4", "mdf" };
  return extensions;
}

bool DataLoadMF4::readDataFromFile(PJ::FileLoadInfo* fileload_info,
                                   PJ::PlotDataMapRef& destination)
{
  QProcess process;
  process.setProgram(pythonExecutable());
  process.setArguments({ QStringLiteral("-u"), QStringLiteral("-c"), metadataBridgeScript(),
                         fileload_info->filename });
  process.setProcessChannelMode(QProcess::SeparateChannels);

  QProgressDialog progress_dialog(QStringLiteral("Scanning MF4/MDF channel metadata..."),
                                  QStringLiteral("Cancel"), 0, 0, nullptr);
  progress_dialog.setWindowTitle(QStringLiteral("Scanning MF4/MDF"));
  progress_dialog.setWindowModality(Qt::ApplicationModal);
  progress_dialog.show();

  process.start();
  if (!process.waitForStarted())
  {
    QMessageBox::warning(nullptr, QStringLiteral("MF4 loader"),
                         QStringLiteral("Failed to start Python executable '%1'.\n\n"
                                        "Set PJ_ASAMMDF_PYTHON to the Python executable "
                                        "that has the 'asammdf' package installed.")
                             .arg(pythonExecutable()));
    return false;
  }

  QByteArray pending_stdout;
  QString error_message;
  size_t loaded_channels = 0;

  auto consume_stdout = [&]() -> bool {
    pending_stdout += process.readAllStandardOutput();
    while (true)
    {
      const int newline_index = pending_stdout.indexOf('\n');
      if (newline_index < 0)
      {
        break;
      }
      const QByteArray line = pending_stdout.left(newline_index).trimmed();
      pending_stdout.remove(0, newline_index + 1);
      if (!line.isEmpty() &&
          !processMetadataLine(line, destination, error_message, loaded_channels))
      {
        return false;
      }
    }
    return true;
  };

  while (process.state() != QProcess::NotRunning)
  {
    process.waitForReadyRead(100);
    if (!consume_stdout())
    {
      process.kill();
      process.waitForFinished(1000);
      QMessageBox::warning(nullptr, QStringLiteral("MF4 loader"), error_message);
      return false;
    }

    progress_dialog.setLabelText(
        QStringLiteral("Scanning MF4/MDF channel metadata...\n%1 numeric channels")
            .arg(loaded_channels));
    QApplication::processEvents();

    if (progress_dialog.wasCanceled())
    {
      process.kill();
      process.waitForFinished(1000);
      return false;
    }
  }

  if (!consume_stdout())
  {
    QMessageBox::warning(nullptr, QStringLiteral("MF4 loader"), error_message);
    return false;
  }
  if (!pending_stdout.trimmed().isEmpty() &&
      !processMetadataLine(pending_stdout.trimmed(), destination, error_message,
                           loaded_channels))
  {
    QMessageBox::warning(nullptr, QStringLiteral("MF4 loader"), error_message);
    return false;
  }

  const QByteArray stderr_output = process.readAllStandardError().trimmed();
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
  {
    const QString details = QString::fromLocal8Bit(stderr_output);
    QMessageBox::warning(nullptr, QStringLiteral("MF4 loader"),
                         details.isEmpty() ? QStringLiteral("MF4 metadata scan failed.")
                                           : details);
    return false;
  }

  if (loaded_channels == 0)
  {
    QMessageBox::warning(nullptr, QStringLiteral("MF4 loader"),
                         QStringLiteral("No numeric MF4/MDF channels were found."));
    return false;
  }

  return true;
}
