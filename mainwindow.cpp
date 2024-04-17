#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QtBluetooth>

#include <QDebug>
#include <QMetaObject>
#include <QTimer>

#if QT_CONFIG(permissions)
#include <QPermissions>

#include <QGuiApplication>
#endif

using namespace Qt::StringLiterals;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    discoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &MainWindow::addDevice);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &MainWindow::deviceScanError);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &MainWindow::deviceScanFinished);
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled,
            this, &MainWindow::deviceScanFinished);
    //! [les-devicediscovery-1]


    setUpdate(u"Search"_s);
}

MainWindow::~MainWindow()
{
    disconnectFromDevice();

    qDeleteAll(devices);
    qDeleteAll(m_services);
    qDeleteAll(m_characteristics);
    devices.clear();
    m_services.clear();
    m_characteristics.clear();

    delete ui;
}


void MainWindow::on_btnIniciarBusca_clicked()
{
    startDeviceDiscovery();
}

void MainWindow::startDeviceDiscovery()
{
    qDeleteAll(devices);
    devices.clear();
    emit devicesUpdated();

    //! [les-devicediscovery-2]
    discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    //! [les-devicediscovery-2]

    if (discoveryAgent->isActive()) {
        setUpdate(u"Stop"_s);
        m_deviceScanState = true;
        Q_EMIT stateChanged();
    }
}

void MainWindow::stopDeviceDiscovery()
{
    if (discoveryAgent->isActive())
        discoveryAgent->stop();
}

//! [les-devicediscovery-3]
void MainWindow::addDevice(const QBluetoothDeviceInfo &info)
{
    if (info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        auto devInfo = new DeviceInfo(info);
        auto it = std::find_if(devices.begin(), devices.end(),
                               [devInfo](DeviceInfo *dev) {
                                   return devInfo->getAddress() == dev->getAddress();
                               });
        if (it == devices.end()) {
            devices.append(devInfo);
            //setUpdate(devInfo->getName() + devInfo->getAddress());
        } else {
            auto oldDev = *it;
            *it = devInfo;
            delete oldDev;
        }
        //emit devicesUpdated();
    }
}
//! [les-devicediscovery-3]

void MainWindow::deviceScanFinished()
{
    ui->dropDispositivos->clear();

    m_deviceScanState = false;
    emit stateChanged();
    if (devices.isEmpty())
        setUpdate(u"No Low Energy devices found..."_s);
    else{
        setUpdate(u"Done! Scan Again!"_s);
        qDebug() << "Achou " << QString::number(devices.count()) << " dispositivos";

        foreach (const DeviceInfo *dev, devices){

            const QString nome = dev->getName() + "\t" + dev->getAddress();
            qDebug() << nome;

            ui->dropDispositivos->addItem(nome);
        }
    }
}

QVariant MainWindow::getDevices()
{
    return QVariant::fromValue(devices);
}

QVariant MainWindow::getServices()
{
    return QVariant::fromValue(m_services);
}

QVariant MainWindow::getCharacteristics()
{
    return QVariant::fromValue(m_characteristics);
}

QString MainWindow::getUpdate()
{
    return m_message;
}

void MainWindow::scanServices(const QString &address)
{
    // We need the current device for service discovery.

    for (auto d: std::as_const(devices)) {
        if (auto device = qobject_cast<DeviceInfo *>(d)) {
            if (device->getAddress() == address) {
                currentDevice.setDevice(device->getDevice());
                break;
            }
        }
    }

    if (!currentDevice.getDevice().isValid()) {
        qWarning() << "Not a valid device";
        return;
    }

    qDeleteAll(m_characteristics);
    m_characteristics.clear();
    emit characteristicsUpdated();
    qDeleteAll(m_services);
    m_services.clear();
    emit servicesUpdated();

    setUpdate(u"Back\n(Connecting to device...)"_s);

    if (controller && m_previousAddress != currentDevice.getAddress()) {
        controller->disconnectFromDevice();
        delete controller;
        controller = nullptr;
    }

    //! [les-controller-1]
    if (!controller) {
        // Connecting signals and slots for connecting to LE services.
        controller = QLowEnergyController::createCentral(currentDevice.getDevice(), this);
        connect(controller, &QLowEnergyController::connected,
                this, &MainWindow::deviceConnected);
        connect(controller, &QLowEnergyController::errorOccurred, this, &MainWindow::errorReceived);
        connect(controller, &QLowEnergyController::disconnected,
                this, &MainWindow::deviceDisconnected);
        connect(controller, &QLowEnergyController::serviceDiscovered,
                this, &MainWindow::addLowEnergyService);
        connect(controller, &QLowEnergyController::discoveryFinished,
                this, &MainWindow::serviceScanDone);
    }

    if (isRandomAddress())
        controller->setRemoteAddressType(QLowEnergyController::RandomAddress);
    else
        controller->setRemoteAddressType(QLowEnergyController::PublicAddress);
    controller->connectToDevice();
    //! [les-controller-1]

    m_previousAddress = currentDevice.getAddress();
}

void MainWindow::addLowEnergyService(const QBluetoothUuid &serviceUuid)
{
    QLowEnergyService *service = controller->createServiceObject(serviceUuid);
    if (!service) {
        qWarning() << "Cannot create service for uuid";
        return;
    }
    QBluetoothUuid meuServicoId("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    auto serv = new ServiceInfo(service);

    if(service && serviceUuid.toString() == meuServicoId.toString()){
        qDebug() << "Achou o serviço do ESP32";

        connect(service, &QLowEnergyService::stateChanged, this, &MainWindow::serviceStateChanged);
        connect(service, &QLowEnergyService::characteristicChanged, this, &MainWindow::mudouCaracteristica);
        connect(service, &QLowEnergyService::descriptorWritten, this, &MainWindow::confirmedDescriptorWrite);
        service->discoverDetails();
        servicoEscolhido = serv;
    }

    m_services.append(serv);

    emit servicesUpdated();
}

void MainWindow::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::RemoteServiceDiscovering:
        qDebug() << "Buscando servicos";
        break;
    case QLowEnergyService::RemoteServiceDiscovered:
    {
        qDebug() << "Servico encontrados";

        const QLowEnergyCharacteristic hrChar = servicoEscolhido->service()->characteristic(QBluetoothUuid(QBluetoothUuid("917ae0b3-c9c0-4d40-87b9-a56eae0c8193")));
        if (!hrChar.isValid()) {
            qDebug() << "Caracteristica invalida";
            break;
        }

        notificacao = hrChar.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (notificacao.isValid())
            servicoEscolhido->service()->writeDescriptor(notificacao, QByteArray::fromHex("0100"));
        else{
            qDebug() << "Caracteristica diferente";
        }

        break;
    }
    default:
        //nothing for now
        break;
    }

    //emit aliveChanged();
}

void MainWindow::confirmedDescriptorWrite(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue)
{
    qDebug() << "Executou o  confirmedDescriptor";
}

void MainWindow::serviceScanDone()
{
    setUpdate(u"Back\n(Service scan done!)"_s);
    ui->dropServicos->clear();
    for(auto s: m_services){
        QString nome = s->getName() + s->getUuid();
        ui->dropServicos->addItem(nome);
    }
    // force UI in case we didn't find anything
    if (m_services.isEmpty())
        emit servicesUpdated();
}

void MainWindow::mudouCaracteristica(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue) {
    qDebug() << "Characteristic changed:"
             << characteristic.uuid()
             << "New value:" << newValue;
}

void MainWindow::connectToService(const QString &uuid)
{
    QLowEnergyService *service = nullptr;
    for (auto s: std::as_const(m_services)) {
        auto serviceInfo = qobject_cast<ServiceInfo *>(s);
        if (!serviceInfo)
            continue;

        if (serviceInfo->getUuid() == uuid) {
            service = serviceInfo->service();
            break;
        }
    }

    if (!service)
        return;

    qDeleteAll(m_characteristics);
    m_characteristics.clear();
    ui->dropCaracteristicas->clear();
    emit characteristicsUpdated();

    if (service->state() == QLowEnergyService::RemoteService) {
        //! [les-service-3]
        //!
        qDebug() << "Conectando servico de caracteristica";

        connect(service, &QLowEnergyService::stateChanged,
                this, &MainWindow::serviceDetailsDiscovered);

        connect(service, &QLowEnergyService::characteristicChanged,
                this, &MainWindow::mudouCaracteristica);


        service->discoverDetails();
        setUpdate(u"Back\n(Discovering details...)"_s);
        //! [les-service-3]
        return;
    }else{
        qDebug() << "Outro tipo de serviço";
    }

    //discovery already done
    const QList<QLowEnergyCharacteristic> chars = service->characteristics();
    for (const QLowEnergyCharacteristic &ch : chars) {
        auto cInfo = new CharacteristicInfo(ch);
        m_characteristics.append(cInfo);
    }

    for(auto item: m_characteristics){
        QString nome = item->getName() + item->getUuid();

        ui->dropCaracteristicas->addItem(nome);
    }

    QTimer::singleShot(0, this, &MainWindow::characteristicsUpdated);
}

void MainWindow::deviceConnected()
{
    setUpdate(u"Back\n(Discovering services...)"_s);
    connected = true;
    //! [les-service-2]
    controller->discoverServices();
    //! [les-service-2]
}

void MainWindow::errorReceived(QLowEnergyController::Error /*error*/)
{
    qWarning() << "Error: " << controller->errorString();
    setUpdate(u"Back\n(%1)"_s.arg(controller->errorString()));
}

void MainWindow::setUpdate(const QString &message)
{
    m_message = message;
    qDebug() << "LOG: " << message;
    emit updateChanged();
}

void MainWindow::disconnectFromDevice()
{
    // UI always expects disconnect() signal when calling this signal
    // TODO what is really needed is to extend state() to a multi value
    // and thus allowing UI to keep track of controller progress in addition to
    // device scan progress

    if (controller->state() != QLowEnergyController::UnconnectedState)
        controller->disconnectFromDevice();
    else
        deviceDisconnected();
}

void MainWindow::deviceDisconnected()
{
    qWarning() << "Disconnect from device";
    emit disconnected();
}

void MainWindow::serviceDetailsDiscovered(QLowEnergyService::ServiceState newState)
{
    qDebug() << "Chamou o serviceDetailsDiscovered";

    if (newState != QLowEnergyService::RemoteServiceDiscovered) {
        // do not hang in "Scanning for characteristics" mode forever
        // in case the service discovery failed
        // We have to queue the signal up to give UI time to even enter
        // the above mode
        if (newState != QLowEnergyService::RemoteServiceDiscovering) {
            QMetaObject::invokeMethod(this, "characteristicsUpdated",
                                      Qt::QueuedConnection);
        }
        return;
    }

    auto service = qobject_cast<QLowEnergyService *>(sender());
    if (!service)
        return;



    //! [les-chars]
    const QList<QLowEnergyCharacteristic> chars = service->characteristics();
    for (const QLowEnergyCharacteristic &ch : chars) {
        auto cInfo = new CharacteristicInfo(ch);

        QLowEnergyCharacteristic hrChar = cInfo->getCharacteristic();
        service->writeDescriptor(hrChar.descriptor(QBluetoothUuid()), QByteArray::fromHex("0100"));

        m_characteristics.append(cInfo);

        QLowEnergyDescriptor notification = cInfo->getCharacteristic().descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);

        if (!notification.isValid()) {
            qDebug() << "QLowEnergyDescriptor not valid";
        }

        connect(servicoEscolhido->service(), &QLowEnergyService::characteristicChanged, this,
                &MainWindow::mudouCaracteristica);

        servicoEscolhido->service()->writeDescriptor(notification, QByteArray::fromHex("0100")
                                                     );
    }
    //! [les-chars]

    emit characteristicsUpdated();
}

void MainWindow::deviceScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError) {
        setUpdate(u"The Bluetooth adaptor is powered off, power it on before doing discovery."_s);
    } else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError) {
        setUpdate(u"Writing or reading from the device resulted in an error."_s);
    } else {
        //static QMetaEnum qme = discoveryAgent->metaObject()->enumerator(
            //discoveryAgent->metaObject()->indexOfEnumerator("Error"));

        //setUpdate(u"Error: "_s + QLatin1StringView(qme.valueToKey(error)));
    }

    m_deviceScanState = false;
    emit stateChanged();
}

bool MainWindow::state()
{
    return m_deviceScanState;
}

bool MainWindow::hasControllerError() const
{
    return (controller && controller->error() != QLowEnergyController::NoError);
}

bool MainWindow::isRandomAddress() const
{
    return randomAddress;
}

void MainWindow::setRandomAddress(bool newValue)
{
    randomAddress = newValue;
    emit randomAddressChanged();
}


void MainWindow::on_btConectar_clicked()
{
    int indice = ui->dropDispositivos->currentIndex();
    DeviceInfo *dispositivoSelecionado = devices[indice];

    scanServices(dispositivoSelecionado->getAddress());
}


void MainWindow::on_btScanCaract_clicked()
{
    int indice = ui->dropServicos->currentIndex();
    ServiceInfo *servicoSelecionado = m_services[indice];
    servicoEscolhido = servicoSelecionado;
    connectToService(servicoSelecionado->getUuid());
}


void MainWindow::on_btLerCaracteristica_clicked()
{

    int indice = ui->dropCaracteristicas->currentIndex();
    CharacteristicInfo *caracteristicaSelecionado = m_characteristics[indice];




    QByteArray a = caracteristicaSelecionado->getCharacteristic().value();
    QString result;
    if (a.isEmpty()) {
        result = u"<none>"_s;
        ui->lbValorCaracteristica->setText(result);
        return;
    }

    result = a;
    result += '\n'_L1;
    result += a.toHex();
    ui->lbValorCaracteristica->setText(result);
}

