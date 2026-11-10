#include "tools/virtual_motor/app/gui/QtRigStore.hpp"
#include <QSettings>

namespace virtual_motor::app
{
    std::vector<MotorSpec> QtRigStore::Load()
    {
        QSettings settings;
        std::vector<MotorSpec> motors;
        const auto count = settings.beginReadArray("motors");
        for (int index = 0; index != count; ++index)
        {
            settings.setArrayIndex(index);
            motors.push_back(MotorSpec{ settings.value("bus", "default").toString().toStdString(),
                static_cast<uint8_t>(settings.value("node").toUInt()),
                settings.value("plant", "teknic").toString().toStdString(),
                StartMode::keepMemory,
                { settings.value("currentOffsetA", 0.0).toFloat(), settings.value("currentOffsetB", 0.0).toFloat(), settings.value("currentOffsetC", 0.0).toFloat() } });
        }
        settings.endArray();
        return motors;
    }

    void QtRigStore::Save(const std::vector<MotorSpec>& motors)
    {
        QSettings settings;
        settings.beginWriteArray("motors", static_cast<int>(motors.size()));
        for (int index = 0; index != static_cast<int>(motors.size()); ++index)
        {
            settings.setArrayIndex(index);
            settings.setValue("bus", QString::fromStdString(motors[index].bus));
            settings.setValue("node", motors[index].node);
            settings.setValue("plant", QString::fromStdString(motors[index].plant));
            settings.setValue("currentOffsetA", motors[index].currentOffset[0]);
            settings.setValue("currentOffsetB", motors[index].currentOffset[1]);
            settings.setValue("currentOffsetC", motors[index].currentOffset[2]);
        }
        settings.endArray();
    }
}
