// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#include "Shooter.hpp"

#include <cfloat>
#include <cmath>

Shooter::Shooter(uint32_t motor1, uint32_t motor2, uint32_t encChannel,
                 uint32_t encTeeth, float encGearRatio)
    : m_shooterMotor1(motor1),
      m_shooterMotor2(motor2),
      m_shooterEncoder(encChannel, encTeeth, encGearRatio),
      m_shooterPID(0.f, 0.f, 0.f, 1.f / maxSpeed, this, this, 0.04f),
      m_isShooting(false),
      m_firstApproach(false),
      m_controlMode(Manual),
      m_negativeOutputAllowed(true),
      m_P(m_shooterPID.GetP()),
      m_I(m_shooterPID.GetI()),
      m_D(m_shooterPID.GetD()),
      m_F(m_shooterPID.GetF()),
      m_setpoint(0.f) {
    m_shooterPID.SetOutputRange(-1.f, 1.f);
    m_shooterPID.SetTolerance(0.f);

    m_shooterPID.SetSetpoint(0.f);

    m_shooterEncoder.start();
    m_shooterPID.Enable();
    start();
}

Shooter::~Shooter() {}

void Shooter::start() {
    m_shooterPID.Enable();
    m_shooterPID.SetSetpoint(m_setpoint);

    m_isShooting = true;
    m_firstApproach = true;
}

void Shooter::stop() {
    m_shooterPID.Reset();

    m_isShooting = false;
}

bool Shooter::isShooting() { return m_isShooting; }

bool Shooter::isReady() {
    return std::fabs(getRPM() - m_setpoint) < 100 && m_isShooting;
}

void Shooter::setRPM(float wheelSpeed) {
    if (m_isShooting) {
        m_setpoint = wheelSpeed;
        m_shooterPID.SetSetpoint(m_setpoint);
    }
}

void Shooter::setScale(float scaleFactor) {
    if (m_isShooting) {
        // Limit 'scaleFactor' to a value between 0 and 1 inclusive
        if (scaleFactor < 0.f) {
            scaleFactor = 0.f;
        }
        if (scaleFactor > 1.f) {
            scaleFactor = 1.f;
        }

        m_setpoint = scaleFactor * maxSpeed;
        m_shooterPID.SetSetpoint(m_setpoint);
    }
}

float Shooter::getRPM() { return m_shooterEncoder.getFilterRPM(); }

float Shooter::getTargetRPM() { return m_setpoint; }

float Shooter::getRawRPM() { return m_shooterEncoder.getCurrentRPM(); }

void Shooter::updateEncoderFilter(double Q, double R) {
    m_shooterEncoder.setFilterQ(Q);
    m_shooterEncoder.setFilterR(R);
    m_shooterEncoder.resetFilter();
}

bool Shooter::negativeOutputAllowed() { return m_negativeOutputAllowed; }

void Shooter::setControlMode(ControlMode mode) {
    m_controlMode = mode;

    if (mode == Manual) {
        m_shooterPID.SetPID(0.f, 0.f, 0.f, m_F);
    } else {
        m_shooterPID.SetPID(m_P, m_I, m_D, 0.f);
    }
}

Shooter::ControlMode Shooter::getControlMode() { return m_controlMode; }

void Shooter::setPID(float p, float i, float d) {
    m_P = p;
    m_I = i;
    m_D = d;

    // Updates PID constants for PIDController object internally
    setControlMode(getControlMode());
}

double Shooter::PIDGet() {
    if (m_shooterEncoder.getCurrentRPM() < 100.f && m_negativeOutputAllowed) {
        m_shooterPID.SetOutputRange(0.f, 1.f);
        m_negativeOutputAllowed = false;
    } else if (m_shooterEncoder.getCurrentRPM() >= 100.f &&
               !m_negativeOutputAllowed) {
        m_shooterPID.SetOutputRange(-1.f, 1.f);
        m_negativeOutputAllowed = true;
    }

    return m_shooterEncoder.getFilterRPM();
}

void Shooter::PIDWrite(float output) {
    /* Ouputs are negated because the motor controllers require a negative
     * number to make the shooter wheel spin in the correct direction
     */
    switch (m_controlMode) {
        case PID: {
            m_shooterMotor1.Set(-output);
            m_shooterMotor2.Set(-output);

            break;
        }

        case BangBang: {
            if (m_shooterEncoder.getFilterRPM() >= m_setpoint) {
                m_shooterMotor1.Set(0.f);
                m_shooterMotor2.Set(0.f);
            } else {
                m_shooterMotor1.Set(-1.f);
                m_shooterMotor2.Set(-1.f);
            }

            break;
        }

        /* The only non-zero term in "Manual" is F, which turns off error
         * correction and responds only to the input given by the user.
         */
        case Manual: {
            m_shooterMotor1.Set(-output);
            m_shooterMotor2.Set(-output);

            break;
        }
    }
}