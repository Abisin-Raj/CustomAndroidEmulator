#pragma once

#include <QObject>
#include "event_types.h"

class EventDispatcher : public QObject {
    Q_OBJECT
public:
    explicit EventDispatcher(QObject* parent = nullptr);
    ~EventDispatcher() override = default;

    void dispatchEvent(const BootEvent& event);
    void dispatchStateChange(BootState state);

signals:
    void eventDispatched(const BootEvent& event);
    void stateChanged(BootState state);
};
