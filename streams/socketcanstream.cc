#include "tools/cabana/streams/socketcanstream.h"

#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QThread>

SocketCanStream::SocketCanStream(QObject *parent, SocketCanStreamConfig config_) : config(config_), LiveStream(parent)
{
  if (!available())
  {
    throw std::runtime_error("SocketCAN plugin not available");
  }

  qDebug() << "Connecting to SocketCAN device" << config.device
           << "CAN FD:" << config.enableCanFD
           << "BRS:" << config.enableBRS;
  if (!connect())
  {
    throw std::runtime_error("Failed to connect to SocketCAN device");
  }
}

bool SocketCanStream::available()
{
  return QCanBus::instance()->plugins().contains("socketcan");
}

bool SocketCanStream::connect()
{
  // Connecting might generate some warnings about missing socketcan/libsocketcan libraries
  // These are expected and can be ignored, we don't need the advanced features of libsocketcan
  QString errorString;
  device.reset(QCanBus::instance()->createDevice("socketcan", config.device, &errorString));

  if (!device)
  {
    qDebug() << "Failed to create SocketCAN device" << errorString;
    return false;
  }

  // Configure CAN FD settings if enabled
  if (config.enableCanFD)
  {
    device->setConfigurationParameter(QCanBusDevice::CanFdKey, true);
    qDebug() << "Enabled CAN FD support";

    if (config.enableBRS)
    {
      device->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000);      // Nominal bit rate
      device->setConfigurationParameter(QCanBusDevice::DataBitRateKey, 2000000); // Data bit rate for CAN FD
      qDebug() << "Enabled Bit Rate Switch (BRS)";
    }
  }
  else
  {
    device->setConfigurationParameter(QCanBusDevice::CanFdKey, false);
    device->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000); // Standard CAN bit rate
  }

  if (!device->connectDevice())
  {
    qDebug() << "Failed to connect to device:" << device->errorString();
    return false;
  }

  return true;
}

void SocketCanStream::streamThread()
{
  while (!QThread::currentThread()->isInterruptionRequested())
  {
    QThread::msleep(1);

    auto frames = device->readAllFrames();
    if (frames.size() == 0)
      continue;

    MessageBuilder msg;
    auto evt = msg.initEvent();
    auto canData = evt.initCan(frames.size());

    for (uint i = 0; i < frames.size(); i++)
    {
      if (!frames[i].isValid())
        continue;

      canData[i].setAddress(frames[i].frameId());
      canData[i].setSrc(0);

      auto payload = frames[i].payload();
      canData[i].setDat(kj::arrayPtr((uint8_t *)payload.data(), payload.size()));

      // Log CAN FD frame information for debugging
      if (config.enableCanFD && frames[i].hasFlexibleDataRateFormat())
      {
        // qDebug() << "Received CAN FD frame - ID:" << Qt::hex << frames[i].frameId()
        //          << "Size:" << payload.size()
        //          << "BRS:" << frames[i].hasBitrateSwitch()
        //          << "ESI:" << frames[i].hasErrorStateIndicator();
      }
    }

    handleEvent(capnp::messageToFlatArray(msg));
  }
}

AbstractOpenStreamWidget *SocketCanStream::widget(AbstractStream **stream)
{
  return new OpenSocketCanWidget(stream);
}

OpenSocketCanWidget::OpenSocketCanWidget(AbstractStream **stream) : AbstractOpenStreamWidget(stream)
{
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->addStretch(1);

  QFormLayout *form_layout = new QFormLayout();

  // Device selection
  QHBoxLayout *device_layout = new QHBoxLayout();
  device_edit = new QComboBox();
  device_edit->setFixedWidth(300);
  device_layout->addWidget(device_edit);

  QPushButton *refresh = new QPushButton(tr("Refresh"));
  refresh->setFixedWidth(100);
  device_layout->addWidget(refresh);
  form_layout->addRow(tr("Device"), device_layout);

  // CAN FD options
  can_fd_checkbox = new QCheckBox(tr("Enable CAN FD"));
  can_fd_checkbox->setChecked(config.enableCanFD);
  form_layout->addRow(tr("Protocol"), can_fd_checkbox);

  brs_checkbox = new QCheckBox(tr("Enable Bit Rate Switch (BRS)"));
  brs_checkbox->setChecked(config.enableBRS);
  brs_checkbox->setEnabled(config.enableCanFD);
  form_layout->addRow(tr("Options"), brs_checkbox);

  main_layout->addLayout(form_layout);
  main_layout->addStretch(1);

  // Connect signals
  QObject::connect(refresh, &QPushButton::clicked, this, &OpenSocketCanWidget::refreshDevices);
  QObject::connect(device_edit, &QComboBox::currentTextChanged, this, [=]
                   { config.device = device_edit->currentText(); });
  QObject::connect(can_fd_checkbox, &QCheckBox::toggled, this, [=](bool checked)
                   { 
    config.enableCanFD = checked;
    brs_checkbox->setEnabled(checked);
    if (!checked) {
      brs_checkbox->setChecked(false);
      config.enableBRS = false;
    } });
  QObject::connect(brs_checkbox, &QCheckBox::toggled, this, [=](bool checked)
                   { config.enableBRS = checked; });

  // Populate devices
  refreshDevices();
}

void OpenSocketCanWidget::refreshDevices()
{
  device_edit->clear();
  for (auto device : QCanBus::instance()->availableDevices(QStringLiteral("socketcan")))
  {
    device_edit->addItem(device.name());
  }
}

bool OpenSocketCanWidget::open()
{
  try
  {
    *stream = new SocketCanStream(qApp, config);
  }
  catch (std::exception &e)
  {
    QMessageBox::warning(nullptr, tr("Warning"), tr("Failed to connect to SocketCAN device: '%1'").arg(e.what()));
    return false;
  }
  return true;
}