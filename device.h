#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QBasicTimer>
#include <unordered_map>
#include <tuple>
#include <deconz.h>
#include "resource.h"

class Event;
class Device;
using DeviceKey = uint64_t; //! uniqueId for an Device, MAC address for physical devices
typedef void (*DeviceStateHandler)(Device *, const Event &);

void DEV_InitStateHandler(Device *device, const Event &event);
void DEV_IdleStateHandler(Device *device, const Event &event);
void DEV_NodeDescriptorStateHandler(Device *device, const Event &event);
void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event);
void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event);
void DEV_ModelIdStateHandler(Device *device, const Event &event);
void DEV_GetDeviceDescriptionHandler(Device *device, const Event &event);

/*! \class Device

    A generic per device supervisor for routers and end-devices.

    This class doesn't and MUST not know anything specific about devices.
    Device specific details are defined in device description files.

    As a starting point a Device only knows the MAC address called `DeviceKey` in this context.

    Each Device has a event driven state machine. As a side effect it is self healing, meaning that
    every missing piece like ZCL attributes or ZDP descriptors will be queried automatically.
    If an error or timeout occurs the process is retried later on.

    For sleeping end-devices the event/awake notifies to the state machine that the
    device is listening. Based on this even deep sleepers can be reached. Note that the event
    is not emitted on each Rx from an device but only when it is certain that the devices is listening
    e.g. MAC Data Requests, specific commands, Poll Control Cluster Checkings, etc.

    Currently implemented:

    - ZDP Node Descriptor
    - ZDP Active Endpoints
    - ZDP Simple Descriptors
    - ZCL ModelId

    TODO

    Configurations like bindings and ZCL attribute reporting are maintained and verified continously. These may
    be specified in device description files or are configured dynamically via REST-API, e.g. a switch controls
    a certain group.

    A Device maintains sub-resources which may represent lights, sensors or any other device. The device state given
    by the REST-API like on/off, brightness or thermostat configuration is kept in RecourceItems per sub-device.
    The state machine continiously verifies that a given state will be set, this is different from the former and
    common approach of fire commands and hope for the best.

    Commands MAY have a time to life (TTL) for example to not switch off a light after 3 hours when it becomes reachable.

    This class doesn't know much about ZCL attributes, it should operate mainly on ResourceItems which encapsulate
    how underlying ZCL attributes are written, parsed or queried. The same Resource item, like state/temperature might
    have a different configuration between devices. All this class sees is a state/temperature item or more precisely
    — just a ResouceItem in a certain state.
 */
class Device : public QObject,
               public Resource
{
    Q_OBJECT
public:
    explicit Device(DeviceKey key, QObject *parent = nullptr);
    void addSubDevice(const Resource *sub);
    DeviceKey key() const { return m_deviceKey; }
    const deCONZ::Node *node() const { return m_node; }

    void handleEvent(const Event &event);
    void setState(DeviceStateHandler state);
    void startStateTimer(int IntervalMs);
    void stopStateTimer();
    void timerEvent(QTimerEvent *event) override;

    std::vector<Resource*> subDevices() const;

    // following handlers need access to private members (friend functions)
    friend void DEV_InitStateHandler(Device *device, const Event &event);
    //friend void DEV_ModelIdStateHandler(Device *device, const Event &event);


private:
    Device(); // not accessible
    /*! sub-devices are not yet referenced via pointers since these may become dangling.
        This is a helper to query the actual sub-device Resource* on demand.
    */
    std::vector<std::tuple<QString, const char*>> m_subDevices;
    const deCONZ::Node *m_node = nullptr; //! a reference to the deCONZ core node
    DeviceKey m_deviceKey = 0; //! for physical devices this is the MAC address
    DeviceStateHandler m_state = nullptr; //! the currently active state handler function
    QBasicTimer m_timer; //! internal single shot timer
};

using DeviceContainer = std::unordered_map<DeviceKey, Device*>;

/*! Returns a device for a given \p key.

    If the device doesn't exist yet it will be created.
    This operation is very fast at constant cost O(1) no matter how many \p devices are present.

    \param parent - must be DeRestPluginPrivate instance
    \param devices - the container which contains the device
    \param key - unique identifier for a device (MAC address for physical devices)
 */
Device *getOrCreateDevice(QObject *parent, DeviceContainer &devices, DeviceKey key);

#endif // DEVICE_H
