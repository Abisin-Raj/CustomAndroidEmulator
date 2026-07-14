#include "boot_events/event_dispatcher.h"

EventDispatcher::EventDispatcher(QObject* parent)
    : QObject(parent) {
    // Register types so they can be passed via signals/slots
    qRegisterMetaType<BootEvent>("BootEvent");
    qRegisterMetaType<BootState>("BootState");
}

void EventDispatcher::dispatchEvent(const BootEvent& event) {
    emit eventDispatched(event);
}

void EventDispatcher::dispatchStateChange(BootState state) {
    emit stateChanged(state);
}
