// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#pragma once

#include <RobotDrive.h>

#include "../Settings.hpp"

class Encoder;
class PIDController;

class MecanumDrive : public RobotDrive {
public:
    MecanumDrive(SpeedController& frontLeftMotor,
                 SpeedController& rearLeftMotor,
                 SpeedController& frontRightMotor,
                 SpeedController& rearRightMotor, uint32_t flA, uint32_t flB,
                 uint32_t rlA, uint32_t rlB, uint32_t frA, uint32_t frB,
                 uint32_t rrA, uint32_t rrB);

    virtual ~MecanumDrive();

    MecanumDrive(const MecanumDrive&) = delete;
    MecanumDrive& operator=(const MecanumDrive&) = delete;

    typedef enum {
        Omni = 0,
        Strafe,
        Arcade,
        FLpivot,
        FRpivot,
        RLpivot,
        RRpivot
    } DriveMode;

    /* This function assumes field-oriented driving
     * x is the magnitude of x translation of the robot [-1..1]
     * y is the magnitude of y translation of the robot [-1..1]
     * rotation is the magnitude of rotation [-1..1]
     */
    void Drive(float x, float y, float rotation, float gyroAngle = 0.0);

    /* If enabled, squares joystick inputs for fine-tuned driving at low speeds
     * while maintaining max speed
     */
    void SquareInputs(bool squared);

    // Sets joystick deadband
    void SetDeadband(float band);

    void SetDriveMode(DriveMode mode);
    DriveMode GetDriveMode();

    // Makes class use encoders to keep robot driving properly
    void EnableEncoders(bool pidEnabled);

    // Returns true if encoders are enabled
    bool AreEncodersEnabled();

    // Set encoder distances to 0
    void ResetEncoders();

    // Reload PID constants
    void ReloadPID();

    // Returns encoder rates if the encoders are enabled
    double GetFLrate();
    double GetRLrate();
    double GetFRrate();
    double GetRRrate();

    // Returns encoder distances if the encoders are enabled
    double GetFLdist();
    double GetRLdist();
    double GetFRdist();
    double GetRRdist();

    // Returns encoder PID loop setpoints
    double GetFLsetpoint();
    double GetRLsetpoint();
    double GetFRsetpoint();
    double GetRRsetpoint();

    static constexpr double maxWheelSpeed = 89.0;

private:
    Settings m_settings;

    bool m_squaredInputs;
    float m_deadband;
    DriveMode m_driveMode;

    Encoder* m_flEncoder;
    PIDController* m_flPID;

    Encoder* m_rlEncoder;
    PIDController* m_rlPID;

    Encoder* m_frEncoder;
    PIDController* m_frPID;

    Encoder* m_rrEncoder;
    PIDController* m_rrPID;

    bool m_pidEnabled;
};
