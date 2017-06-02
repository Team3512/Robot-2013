// Copyright (c) 2013-2017 FRC Team 3512. All Rights Reserved.

#include "Feeder.hpp"

Feeder::Feeder(uint32_t feedSolenoid, uint32_t guardSolenoid, double feedDelay,
               double guardDelay)
    : m_frisbeeFeed(feedSolenoid),
      m_feedDelay(feedDelay),
      m_frisbeeGuard(guardSolenoid),
      m_guardDelay(guardDelay),
      m_isActivated(false),
      m_numShot(0),
      m_totalToShoot(0) {}

Feeder::~Feeder() {}

void Feeder::activate() {
    // Start process if it's stopped
    if (!m_isActivated) {
        // Make sure the feed actuator is in a known state: the default
        m_frisbeeFeed.Set(false);

        // Lower shooter guard so frisbees can leave
        m_frisbeeGuard.Set(true);

        // Start the delay timers
        m_feedTimer.Start();
        m_guardTimer.Start();

        m_isActivated = true;

        // Reset counters
        m_numShot = 0;
        m_totalToShoot = 0;
    }

    // Increase number of frisbees to shoot before stopping process
    m_totalToShoot++;
}

void Feeder::update() {
    // If frisbee is going to be fed into the shooter
    if (m_isActivated) {
        // If there are still frisbees to shoot
        if (m_numShot < m_totalToShoot) {
            if (m_feedTimer.HasPeriodPassed(m_feedDelay)) {
                // Switch state of solenoid
                m_frisbeeFeed.Set(!m_frisbeeFeed.Get());

                // If feed actuator is now in default position
                if (m_frisbeeFeed.Get() == false) {
                    m_numShot++;
                }
            }
        }

        // If time for 'm_totalShoot' transits and the guard delay has passed
        if (m_guardTimer.Get() >
            m_totalToShoot * (2 * m_feedDelay) + m_guardDelay) {
            m_frisbeeGuard.Set(false);

            // Reset feed timer
            m_feedTimer.Stop();
            m_feedTimer.Reset();

            // Reset guard timer
            m_guardTimer.Stop();
            m_guardTimer.Reset();

            // Process is done, allow it to repeat
            m_isActivated = false;
        }
    }
}

bool Feeder::isFeeding() { return m_isActivated; }

void Feeder::setFeedDelay(double time) { m_feedDelay = time; }

void Feeder::setGuardDelay(double time) { m_guardDelay = time; }
