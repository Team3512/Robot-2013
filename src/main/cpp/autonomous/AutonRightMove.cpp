// Copyright (c) 2013-2020 FRC Team 3512. All Rights Reserved.

#include "Robot.hpp"

void Robot::AutonRightMove() {
    m_flEncoder.Reset();

    SetShooterAngle(ShooterAngle::kLow);

    m_shooter.Enable();
    m_shooter.SetReference(Shooter::kMaxSpeed);

    // Move robot 5 meters sideways
    while (IsAutonomous() && m_flEncoder.GetDistance() < 35.f) {
        m_drive.DriveCartesian(0.8f, 0.0, 0.0, 0.0);

        frc2::Wait(0.1_s);
    }

    // Stop and start rotating to the left
    m_drive.DriveCartesian(0.0, 0.0, 0.0, 0.0);

    auto turnTimeStart = m_autoTime.Get();
    while (m_autoTime.Get() - turnTimeStart < 0.53_s) {
        m_drive.DriveCartesian(0.0, 0.0, -0.5, 0.0);
    }

    // Stop and start shooting
    m_drive.DriveCartesian(0.0, 0.0, 0.0, 0.0);

    // Initialize variables needed for feeding frisbees properly
    auto feedTimeStart = m_autoTime.Get();
    unsigned int shot = 0;

    // Feed frisbees into shooter with a small delay between each
    while (IsAutonomous()) {
        if (m_autoTime.Get() - feedTimeStart > 1.4_s && shot <= 3 &&
            !m_feeder.IsFeeding()) {
            m_feeder.Activate();
            shot++;

            feedTimeStart = m_autoTime.Get();
        }

        m_feeder.Update();
        m_shooter.Update();

        frc2::Wait(0.05_s);
    }
}
