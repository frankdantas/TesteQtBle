#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "characteristicinfo.h"
#include "deviceinfo.h"
#include "serviceinfo.h"


#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>

#include <QList>
#include <QObject>

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QBluetoothDeviceInfo;
class QBluetoothUuid;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QVariant getDevices();
    QVariant getServices();
    QVariant getCharacteristics();
    QString getUpdate();
    bool state();
    bool hasControllerError() const;

    bool isRandomAddress() const;
    void setRandomAddress(bool newValue);

public slots:
    void startDeviceDiscovery();
    void stopDeviceDiscovery();
    void scanServices(const QString &address);

    void connectToService(const QString &uuid);
    void disconnectFromDevice();

private slots:
    void on_btnIniciarBusca_clicked();
    // QBluetoothDeviceDiscoveryAgent related
    void addDevice(const QBluetoothDeviceInfo&);
    void deviceScanFinished();
    void deviceScanError(QBluetoothDeviceDiscoveryAgent::Error);

    // QLowEnergyController realted
    void addLowEnergyService(const QBluetoothUuid &uuid);
    void deviceConnected();
    void errorReceived(QLowEnergyController::Error);
    void serviceScanDone();
    void deviceDisconnected();

    void serviceStateChanged(QLowEnergyService::ServiceState s);
    void confirmedDescriptorWrite(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue);

    // QLowEnergyService related
    void serviceDetailsDiscovered(QLowEnergyService::ServiceState newState);
    void mudouCaracteristica(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue);

    void on_btConectar_clicked();

    void on_btScanCaract_clicked();

    void on_btLerCaracteristica_clicked();

Q_SIGNALS:
    void devicesUpdated();
    void servicesUpdated();
    void characteristicsUpdated();
    void updateChanged();
    void stateChanged();
    void disconnected();
    void randomAddressChanged();

private:
    void setUpdate(const QString &message);
    QBluetoothDeviceDiscoveryAgent *discoveryAgent;
    DeviceInfo currentDevice;
    QList<DeviceInfo *> devices;
    QList<ServiceInfo *> m_services;
    ServiceInfo *servicoEscolhido;
    QList<CharacteristicInfo *> m_characteristics;
    QString m_previousAddress;
    QString m_message;
    bool connected = false;
    QLowEnergyController *controller = nullptr;
    bool m_deviceScanState = false;
    bool randomAddress = false;

    QLowEnergyDescriptor notificacao;

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
