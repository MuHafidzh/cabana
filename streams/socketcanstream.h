#pragma once

#include <memory>

#include <QtSerialBus/QCanBus>
#include <QtSerialBus/QCanBusDevice>
#include <QtSerialBus/QCanBusDeviceInfo>
#include <QComboBox>
#include <QCheckBox>

#include "tools/cabana/streams/livestream.h"

struct SocketCanStreamConfig
{
  QString device = ""; // TODO: support multiple devices/buses at once
  bool enableCanFD = false;
  bool enableBRS = false; // Bit Rate Switch
};

class SocketCanStream : public LiveStream
{
  Q_OBJECT
public:
  SocketCanStream(QObject *parent, SocketCanStreamConfig config_ = {});
  ~SocketCanStream() { stop(); }
  static AbstractOpenStreamWidget *widget(AbstractStream **stream);
  static bool available();

  inline QString routeName() const override
  {
    QString fdStatus = config.enableCanFD ? " (CAN FD)" : " (Classic CAN)";
    return QString("Live Streaming From Socket CAN %1%2").arg(config.device).arg(fdStatus);
  }

protected:
  void streamThread() override;
  bool connect();

  SocketCanStreamConfig config = {};
  std::unique_ptr<QCanBusDevice> device;
};

class OpenSocketCanWidget : public AbstractOpenStreamWidget
{
  Q_OBJECT

public:
  OpenSocketCanWidget(AbstractStream **stream);
  bool open() override;
  QString title() override { return tr("&SocketCAN"); }

private:
  void refreshDevices();

  QComboBox *device_edit;
  QCheckBox *can_fd_checkbox;
  QCheckBox *brs_checkbox;
  SocketCanStreamConfig config = {};
};