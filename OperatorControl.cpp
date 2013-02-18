//=============================================================================
//File Name: OperatorControl.cpp
//Description: Handles driver controls for robot
//Author: FRC Team 3512, Spartatroniks
//=============================================================================

#include <Timer.h>
#include "OurRobot.hpp"
#include "ButtonTracker.hpp"

void OurRobot::OperatorControl() {
    mainCompressor.Start();

    // Turn on blue underglow
    underGlow.Set( Relay::kOn );
    underGlow.Set( Relay::kForward );

    ButtonTracker driveStick1Buttons( 1 );
    ButtonTracker driveStick2Buttons( 2 );
    ButtonTracker shootStickButtons( 3 );

    bool climberMoving = false;

    float joyX = 0.f;
    float joyY = 0.f;
    float joyTwist = 0.f;
    float joyGyro = 0.f;

    // Set initial drive mode
    MecanumDrive::DriveMode driveMode = MecanumDrive::Omni;
    mainDrive.SetDriveMode( driveMode );

    mainDrive.EnableEncoders( true );

    while ( IsEnabled() && IsOperatorControl() ) {
        DS_PrintOut();

        // update "new" value of joystick buttons
        driveStick1Buttons.updateButtons();
        driveStick2Buttons.updateButtons();
        shootStickButtons.updateButtons();

        /* ============== Toggle Shooter Motors ============== */
        // turns shooter on/off
        if ( shootStickButtons.releasedButton( 4 ) ) {
            frisbeeShooter.start();
        }
        else if ( shootStickButtons.releasedButton( 5 ) ) {
            frisbeeShooter.stop();
        }

        if ( frisbeeShooter.isShooting() ) {
            if ( isShooterManual ) { // let driver change shooter speed manually
                frisbeeShooter.setScale( ScaleValue( shootStick.GetZ() ) );
            }
            else {
                frisbeeShooter.setRPM( ScaleValue( shootStick.GetZ() ) * Shooter::maxSpeed );
            }
        }
        else {
            frisbeeShooter.stop();
        }

        // toggle manual RPM setting vs setting with encoder input
        if ( shootStickButtons.releasedButton( 8 ) ) {
            isShooterManual = false;

            frisbeeShooter.enableControl();
        }

        if ( shootStickButtons.releasedButton( 9 ) ) {
            isShooterManual = true;

            frisbeeShooter.disableControl();
        }
        /* =================================================== */

        /* ===== Change shooter angle and speed ===== */
        // Use high shooter angle
        if ( shootStickButtons.releasedButton( 2 ) ) {
            shooterAngle.Set( true );
        }

        // Use low shooter angle
        if ( shootStickButtons.releasedButton( 3 ) ) {
            shooterAngle.Set( false );
        }
        /* ========================================== */

        /* ===== Shoot frisbee ===== */
        /* Don't let a frisbee into the shooter if the shooter wheel isn't
         * spinning, except in the case of a manual override.
         */
        if ( frisbeeShooter.getRPM() > 500 && !isShooterManual ) {
            if ( shootStickButtons.releasedButton( 1 ) ) {
                frisbeeFeeder.activate();
            }
        }

        // Updates state of feed actuators
        frisbeeFeeder.update();
        /* ========================= */

        /* ===== Control climbing mechanism ===== */
        climberMoving = false;

        // Left arm up
        if ( shootStick.GetRawButton( 6 ) ) {
            climberMoving = true;
            leftClimbArm.Set( Relay::kOn );

            leftClimbArm.Set( Relay::kForward );
        }

        // Left arm down
        else if ( shootStick.GetRawButton( 7 ) ) {
            climberMoving = true;
            leftClimbArm.Set( Relay::kOn );

            leftClimbArm.Set( Relay::kReverse );
        }

        // Left arm stop if motors won't both be moving
        else if ( !shootStick.GetRawButton( 8 ) && !shootStick.GetRawButton( 9 ) ) {
            leftClimbArm.Set( Relay::kOff );
        }

        // Right arm up
        if ( shootStick.GetRawButton( 11 ) ) {
            climberMoving = true;
            rightClimbArm.Set( Relay::kOn );

            rightClimbArm.Set( Relay::kForward );
        }

        // Right arm down
        else if ( shootStick.GetRawButton( 10 ) ) {
            climberMoving = true;
            rightClimbArm.Set( Relay::kOn );

            rightClimbArm.Set( Relay::kReverse );
        }

        // Right arm stop if motors won't both be moving
        else if ( !shootStick.GetRawButton( 8 ) && !shootStick.GetRawButton( 9 ) ) {
            rightClimbArm.Set( Relay::kOff );
        }

        // Both arms up
        if ( shootStick.GetRawButton( 8 ) ) {
            climberMoving = true;
            leftClimbArm.Set( Relay::kOn );
            rightClimbArm.Set( Relay::kOn );

            leftClimbArm.Set( Relay::kForward );
            rightClimbArm.Set( Relay::kForward );
        }

        // Both arms down
        else if ( shootStick.GetRawButton( 9 ) ) {
            climberMoving = true;
            leftClimbArm.Set( Relay::kOn );
            rightClimbArm.Set( Relay::kOn );

            leftClimbArm.Set( Relay::kReverse );
            rightClimbArm.Set( Relay::kReverse );
        }

        // If climber shouldn't be moving, make the motors stop
        if ( !climberMoving ) {
            leftClimbArm.Set( Relay::kOff );
            rightClimbArm.Set( Relay::kOff );
        }
        /* ====================================== */

        /* ===== Control gyro ===== */
        // Reset gyro
        if ( driveStick2Buttons.releasedButton( 8 ) ) {
            testGyro.Reset();
        }

        if ( driveStick2Buttons.releasedButton( 5 ) ) {
            isGyroEnabled = true;

            // kForward turns on blue lights
            underGlow.Set( Relay::kOn );
            underGlow.Set( Relay::kForward );
        }

        if ( driveStick2Buttons.releasedButton( 6 ) ) {
            isGyroEnabled = false;

            // kReverse turns on red lights
            underGlow.Set( Relay::kOn );
            underGlow.Set( Relay::kReverse );
        }
        /* ======================== */

        // Enable encoders with PID loops
        if ( driveStick2Buttons.releasedButton( 3 ) ) {
            //mainDrive.EnableEncoders( true );
        }

        // Disable encoders with PID loops
        if ( driveStick2Buttons.releasedButton( 4 ) ) {
            //mainDrive.EnableEncoders( false );
        }

        // Compensate with gyro angle if that's enabled
        if ( isGyroEnabled && driveMode != MecanumDrive::Arcade ) {
            joyGyro = testGyro.GetAngle();
        }
        // Compensate for ArcadeDrive robot being backwards
        else if ( driveMode == MecanumDrive::Arcade ) {
            joyGyro = 90.f;
        }
        else {
            joyGyro = 0.f;
        }

        // While the thumb button is pressed, don't allow rotation
        if ( driveStick2.GetRawButton( 2 ) ) {
            joyTwist = 0.f;
        }
        else {
            joyTwist = driveStick2.GetZ();
        }

        // Set X and Y joystick inputs
        if ( driveMode != MecanumDrive::Arcade ) {
            joyX = driveStick2.GetX();
            joyY = driveStick2.GetY();
        }
        else {
            joyX = driveStick1.GetX();
            joyY = driveStick2.GetY();
        }

        // Cycle through driving modes
        if ( driveStick2Buttons.releasedButton( 7 ) ) {
            if ( driveMode == MecanumDrive::Omni ) {
                driveMode = MecanumDrive::Strafe;
            }
            else if ( driveMode == MecanumDrive::Strafe ) {
                driveMode = MecanumDrive::Arcade;
            }
            else if ( driveMode == MecanumDrive::Arcade ) {
                driveMode = MecanumDrive::Omni;
            }

            // Update drive mode
            mainDrive.SetDriveMode( driveMode );
        }

        /* Pivot around a given wheel, or drive normally if no buttons were
         * pressed
         */
        if ( driveStick2.GetRawButton( 9 ) ) { // FL wheel pivot
            mainDrive.SetDriveMode( MecanumDrive::FLpivot );
        }
        else if ( driveStick2.GetRawButton( 10 ) ) { // FR wheel pivot
            mainDrive.SetDriveMode( MecanumDrive::FRpivot );
        }
        else if ( driveStick2.GetRawButton( 11 ) ) { // RL wheel pivot
            mainDrive.SetDriveMode( MecanumDrive::RLpivot );
        }
        else if ( driveStick2.GetRawButton( 12 ) ) { // RR wheel pivot
            mainDrive.SetDriveMode( MecanumDrive::RRpivot );
        }
        else {
            mainDrive.SetDriveMode( driveMode );
        }

        // If in lower half, go half speed
        if ( ScaleValue( driveStick2.GetTwist() ) < 0.5 ) {
            slowRotate = true;
            joyTwist /= 2.f;
        }
        else {
            slowRotate = false;
        }
        // ^ Goes normal speed if in higher half

        mainDrive.Drive( joyX , joyY , joyTwist , joyGyro );

        Wait( 0.1 );
    }
}
