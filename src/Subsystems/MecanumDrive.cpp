// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#include "MecanumDrive.hpp"

#include <algorithm>
#include <cmath>

#include <CANJaguar.h>
#include <Encoder.h>
#include <SpeedController.h>

#include "../PIDController.hpp"

/* We define our own hypotenuse function because the one on VxWorks 6.3 isn't
 * defined
 */
double hypot2(double x, double y) { return std::sqrt(x * x + y * y); }

MecanumDrive::MecanumDrive(SpeedController& frontLeftMotor,
                           SpeedController& rearLeftMotor,
                           SpeedController& frontRightMotor,
                           SpeedController& rearRightMotor, uint32_t flA,
                           uint32_t flB, uint32_t rlA, uint32_t rlB,
                           uint32_t frA, uint32_t frB, uint32_t rrA,
                           uint32_t rrB)
    : RobotDrive(frontLeftMotor, rearLeftMotor, frontRightMotor,
                 rearRightMotor),
      m_settings("RobotSettings.txt") {
    m_pidEnabled = false;
    m_squaredInputs = false;
    m_deadband = 0.f;
    m_driveMode = Omni;

    /* ===== Initialize encoders ===== */
    m_flEncoder = new Encoder(flA, flB, true);
    m_rlEncoder = new Encoder(rlA, rlB, true);
    m_frEncoder = new Encoder(frA, frB, true);
    m_rrEncoder = new Encoder(rrA, rrB, true);

    float dPerP = 60.f / 250.f;
    m_flEncoder->SetDistancePerPulse(dPerP);
    m_rlEncoder->SetDistancePerPulse(dPerP);
    m_frEncoder->SetDistancePerPulse(dPerP);
    m_rrEncoder->SetDistancePerPulse(dPerP);

    m_flEncoder->SetPIDSourceParameter(Encoder::kRate);
    m_rlEncoder->SetPIDSourceParameter(Encoder::kRate);
    m_frEncoder->SetPIDSourceParameter(Encoder::kRate);
    m_rrEncoder->SetPIDSourceParameter(Encoder::kRate);
    /* =============================== */

    /* ===== Start PID loops for motors ===== */
    float p = std::atof(m_settings.getValueFor("PID_DRIVE_P").c_str());
    float i = std::atof(m_settings.getValueFor("PID_DRIVE_I").c_str());
    float d = std::atof(m_settings.getValueFor("PID_DRIVE_D").c_str());

    m_flPID = new PIDController(p, i, d, m_flEncoder, m_frontLeftMotor);
    m_rlPID = new PIDController(p, i, d, m_rlEncoder, m_rearLeftMotor);
    m_frPID = new PIDController(p, i, d, m_frEncoder, m_frontRightMotor);
    m_rrPID = new PIDController(p, i, d, m_rrEncoder, m_rearRightMotor);

    m_flPID->SetOutputRange(-1, 1);
    m_rlPID->SetOutputRange(-1, 1);
    m_frPID->SetOutputRange(-1, 1);
    m_rrPID->SetOutputRange(-1, 1);
    /* ====================================== */

    // Enable PID loops
    EnableEncoders(true);
}

MecanumDrive::~MecanumDrive() {
    m_flEncoder->Stop();
    m_rlEncoder->Stop();
    m_frEncoder->Stop();
    m_rrEncoder->Stop();

    delete m_flEncoder;
    delete m_rlEncoder;
    delete m_frEncoder;
    delete m_rrEncoder;

    delete m_flPID;
    delete m_rlPID;
    delete m_frPID;
    delete m_rrPID;
}

/**
 * Drive method for Mecanum wheeled robots.
 *
 * A method for driving with Mecanum wheeled robots. There are 4 wheels
 * on the robot, arranged so that the front and back wheels are parallel.
 *
 * This is designed to be directly driven by joystick axes.
 *
 * @param x The speed that the robot should drive in the X direction.
 * [-1.0..1.0]
 * @param y The speed that the robot should drive in the Y direction.
 * This input is inverted to match the forward == -1.0 that joysticks produce.
 * [-1.0..1.0]
 * @param rotation The rate of rotation for the robot that is completely
 * independent of
 * the translation. [-1.0..1.0]
 * @param gyroAngle The current angle reading from the gyro.  Use this to
 * implement field-oriented controls.
 */
void MecanumDrive::Drive(float x, float y, float rotation, float gyroAngle) {
    double xIn = x;
    double yIn = y;

    // Limit values to [-1 .. 1]
    xIn = Limit(xIn);
    yIn = Limit(yIn);
    rotation = Limit(rotation);

    if (m_squaredInputs) {
        if (xIn < 0) {
            xIn = -pow(xIn, 2);
        } else {
            xIn = std::pow(xIn, 2);
        }

        if (yIn < 0) {
            yIn = -pow(yIn, 2);
        } else {
            yIn = std::pow(yIn, 2);
        }

        if (rotation < 0) {
            rotation = -pow(rotation, 2);
        } else {
            rotation = std::pow(rotation, 2);
        }
    }

    // Apply joystick deadband
    if (std::fabs(xIn) < m_deadband) {
        xIn = 0.f;
    }
    if (std::fabs(yIn) < m_deadband) {
        yIn = 0.f;
    }
    if (std::fabs(rotation) < m_deadband) {
        rotation = 0.f;
    }

    // Compenstate for gyro angle
    RotateVector(xIn, yIn, gyroAngle);

    double wheelSpeeds[kMaxNumberOfMotors];

    // Toggle different driving modes
    switch (m_driveMode) {
        case Omni: {
            /* adding "pi / 4" to the angle rotates the force vector for inline
             * mecanum
             * wheels with the internal rollers rotated "pi / 4" radians
             *
             * "pi / 2" is added to the angle to rotate std::sin(0) so it
             * corresponds with
             * driving forward
             */

            wheelSpeeds[kFrontLeftMotor] =
                hypot2(xIn, yIn) *
                    std::sin(std::atan2(yIn, xIn) + 3.f * 3.141592f / 4.f) +
                rotation;
            wheelSpeeds[kFrontRightMotor] =
                -hypot2(xIn, yIn) *
                    std::cos(std::atan2(yIn, xIn) + 3.f * 3.141592f / 4.f) +
                rotation;
            wheelSpeeds[kRearLeftMotor] =
                hypot2(xIn, yIn) *
                    std::cos(std::atan2(yIn, xIn) + 3.f * 3.141592f / 4.f) +
                rotation;
            wheelSpeeds[kRearRightMotor] =
                -hypot2(xIn, yIn) *
                    std::sin(std::atan2(yIn, xIn) + 3.f * 3.141592f / 4.f) +
                rotation;

            break;
        }

        case Strafe: {
            wheelSpeeds[kFrontLeftMotor] =
                xIn * std::sin(3.f * 3.141592f / 4.f) + rotation;
            wheelSpeeds[kFrontRightMotor] =
                -xIn * std::cos(3.f * 3.141592f / 4.f) + rotation;
            wheelSpeeds[kRearLeftMotor] =
                xIn * std::cos(3.f * 3.141592f / 4.f) + rotation;
            wheelSpeeds[kRearRightMotor] =
                -xIn * std::sin(3.f * 3.141592f / 4.f) + rotation;

            break;
        }

        case Arcade: {
            float leftMotorOutput = 0.f;
            float rightMotorOutput = 0.f;

            if (yIn > 0.f) {
                if (xIn > 0.f) {
                    leftMotorOutput = yIn - xIn;
                    rightMotorOutput = std::max(yIn, xIn);
                } else {
                    leftMotorOutput = std::max(yIn, -xIn);
                    rightMotorOutput = yIn + xIn;
                }
            } else {
                if (xIn > 0.f) {
                    leftMotorOutput = -std::max(-yIn, xIn);
                    rightMotorOutput = yIn + xIn;
                } else {
                    leftMotorOutput = yIn - xIn;
                    rightMotorOutput = -std::max(-yIn, -xIn);
                }
            }

            wheelSpeeds[kFrontLeftMotor] = leftMotorOutput;
            wheelSpeeds[kFrontRightMotor] = rightMotorOutput;
            wheelSpeeds[kRearLeftMotor] = leftMotorOutput;
            wheelSpeeds[kRearRightMotor] = rightMotorOutput;

            break;
        }

        case FLpivot: {
            wheelSpeeds[kFrontLeftMotor] = 0.f;
            wheelSpeeds[kFrontRightMotor] = -19.f;
            wheelSpeeds[kRearLeftMotor] = -27.5f;
            wheelSpeeds[kRearRightMotor] = -33.425f / std::sin(3.141592f / 4.f);

            Normalize(wheelSpeeds);

            wheelSpeeds[kFrontLeftMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kFrontRightMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kRearLeftMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kRearRightMotor] *= hypot2(xIn, yIn);

            break;
        }

        case FRpivot: {
            wheelSpeeds[kFrontLeftMotor] = 19.f;
            wheelSpeeds[kFrontRightMotor] = 0.f;
            wheelSpeeds[kRearLeftMotor] = 33.425f / std::sin(3.141592f / 4.f);
            wheelSpeeds[kRearRightMotor] = 27.5f;

            Normalize(wheelSpeeds);

            wheelSpeeds[kFrontLeftMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kFrontRightMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kRearLeftMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kRearRightMotor] *= hypot2(xIn, yIn);

            break;
        }

        case RLpivot: {
            wheelSpeeds[kFrontLeftMotor] = -27.5f;
            wheelSpeeds[kFrontRightMotor] =
                -33.425f / std::sin(3.141592f / 4.f);
            wheelSpeeds[kRearLeftMotor] = 0.f;
            wheelSpeeds[kRearRightMotor] = -19.f;

            Normalize(wheelSpeeds);

            wheelSpeeds[kFrontLeftMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kFrontRightMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kRearLeftMotor] *= hypot2(xIn, yIn);
            wheelSpeeds[kRearRightMotor] *= hypot2(xIn, yIn);

            break;
        }

        case RRpivot: {
            wheelSpeeds[kFrontLeftMotor] = 33.425f / std::sin(3.141592f / 4.f);
            wheelSpeeds[kFrontRightMotor] = 27.5f;
            wheelSpeeds[kRearLeftMotor] = 19.f;
            wheelSpeeds[kRearRightMotor] = 0.f;

            Normalize(wheelSpeeds);

            wheelSpeeds[kFrontLeftMotor] *= hypot2(xIn, yIn) / std::sqrt(2);
            wheelSpeeds[kFrontRightMotor] *= hypot2(xIn, yIn) / std::sqrt(2);
            wheelSpeeds[kRearLeftMotor] *= hypot2(xIn, yIn) / std::sqrt(2);
            wheelSpeeds[kRearRightMotor] *= hypot2(xIn, yIn) / std::sqrt(2);

            break;
        }
    }

    // Normalize wheel speeds and run PID loop if enabled
    Normalize(wheelSpeeds);

    if (m_pidEnabled) {
        m_flPID->SetSetpoint(maxWheelSpeed * wheelSpeeds[kFrontLeftMotor] *
                             m_invertedMotors[kFrontLeftMotor] * m_maxOutput);
        m_frPID->SetSetpoint(maxWheelSpeed * wheelSpeeds[kFrontRightMotor] *
                             m_invertedMotors[kFrontRightMotor] * m_maxOutput);
        m_rlPID->SetSetpoint(maxWheelSpeed * wheelSpeeds[kRearLeftMotor] *
                             m_invertedMotors[kRearLeftMotor] * m_maxOutput);
        m_rrPID->SetSetpoint(maxWheelSpeed * wheelSpeeds[kRearRightMotor] *
                             m_invertedMotors[kRearRightMotor] * m_maxOutput);

        m_safetyHelper->Feed();
    } else {
        UINT8 syncGroup = 0x80;

        m_frontLeftMotor->Set(wheelSpeeds[kFrontLeftMotor] *
                                  m_invertedMotors[kFrontLeftMotor] *
                                  m_maxOutput,
                              syncGroup);
        m_frontRightMotor->Set(wheelSpeeds[kFrontRightMotor] *
                                   m_invertedMotors[kFrontRightMotor] *
                                   m_maxOutput,
                               syncGroup);
        m_rearLeftMotor->Set(wheelSpeeds[kRearLeftMotor] *
                                 m_invertedMotors[kRearLeftMotor] * m_maxOutput,
                             syncGroup);
        m_rearRightMotor->Set(wheelSpeeds[kRearRightMotor] *
                                  m_invertedMotors[kRearRightMotor] *
                                  m_maxOutput,
                              syncGroup);

        CANJaguar::UpdateSyncGroup(syncGroup);

        m_safetyHelper->Feed();
    }
}

void MecanumDrive::SquareInputs(bool squared) { m_squaredInputs = squared; }

void MecanumDrive::SetDeadband(float band) { m_deadband = band; }

void MecanumDrive::SetDriveMode(DriveMode mode) { m_driveMode = mode; }

MecanumDrive::DriveMode MecanumDrive::GetDriveMode() { return m_driveMode; }

void MecanumDrive::EnableEncoders(bool pidEnabled) {
    // If wasn't enabled and is now
    if (!m_pidEnabled && pidEnabled) {
        m_flEncoder->Start();
        m_rlEncoder->Start();
        m_frEncoder->Start();
        m_rrEncoder->Start();

        m_flPID->Enable();
        m_rlPID->Enable();
        m_frPID->Enable();
        m_rrPID->Enable();

        m_pidEnabled = pidEnabled;
    } else if (m_pidEnabled && !pidEnabled) {
        // If was enabled and isn't now
        m_pidEnabled = pidEnabled;

        m_flPID->Reset();
        m_rlPID->Reset();
        m_frPID->Reset();
        m_rrPID->Reset();

        m_flEncoder->Stop();
        m_rlEncoder->Stop();
        m_frEncoder->Stop();
        m_rrEncoder->Stop();
    }
}

// Returns true if encoders are enabled
bool MecanumDrive::AreEncodersEnabled() { return m_pidEnabled; }

void MecanumDrive::ResetEncoders() {
    if (m_pidEnabled) {
        m_flEncoder->Reset();
        m_frEncoder->Reset();
        m_rlEncoder->Reset();
        m_rrEncoder->Reset();
    }
}

void MecanumDrive::ReloadPID() {
    m_settings.update();

    float p = std::atof(m_settings.getValueFor("PID_DRIVE_P").c_str());
    float i = std::atof(m_settings.getValueFor("PID_DRIVE_I").c_str());
    float d = std::atof(m_settings.getValueFor("PID_DRIVE_D").c_str());

    m_flPID->SetPID(p, i, d);
    m_frPID->SetPID(p, i, d);
    m_rlPID->SetPID(p, i, d);
    m_rrPID->SetPID(p, i, d);
}

// Returns encoder values if the encoders are enabled
double MecanumDrive::GetFLrate() {
    if (m_pidEnabled) {
        return m_flEncoder->GetRate();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetRLrate() {
    if (m_pidEnabled) {
        return m_rlEncoder->GetRate();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetFRrate() {
    if (m_pidEnabled) {
        return m_frEncoder->GetRate();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetRRrate() {
    if (m_pidEnabled) {
        return m_rrEncoder->GetRate();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetFLdist() {
    if (m_pidEnabled) {
        return m_flEncoder->GetDistance();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetRLdist() {
    if (m_pidEnabled) {
        return m_rlEncoder->GetDistance();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetFRdist() {
    if (m_pidEnabled) {
        return m_frEncoder->GetDistance();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetRRdist() {
    if (m_pidEnabled) {
        return m_rrEncoder->GetDistance();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetFLsetpoint() {
    if (m_pidEnabled) {
        return m_flPID->GetSetpoint();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetRLsetpoint() {
    if (m_pidEnabled) {
        return m_flPID->GetSetpoint();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetFRsetpoint() {
    if (m_pidEnabled) {
        return m_flPID->GetSetpoint();
    } else {
        return 0.0;
    }
}

double MecanumDrive::GetRRsetpoint() {
    if (m_pidEnabled) {
        return m_flPID->GetSetpoint();
    } else {
        return 0.0;
    }
}