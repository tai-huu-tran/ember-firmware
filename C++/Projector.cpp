//  File:   Projector.cpp
//  Encapsulates the functionality of the printer's projector
//
//  This file is part of the Ember firmware.
//
//  Copyright 2015 Autodesk, Inc. <http://ember.autodesk.com/>
//    
//  Authors:
//  Richard Greene
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  THIS PROGRAM IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL,
//  BUT WITHOUT ANY WARRANTY; WITHOUT EVEN THE IMPLIED WARRANTY OF
//  MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  SEE THE
//  GNU GENERAL PUBLIC LICENSE FOR MORE DETAILS.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.

#include <iostream>
#include <stdexcept>

#include <Magick++.h>

#include <Projector.h>
#include <Logger.h>
#include <Filenames.h>
#include <MessageStrings.h>
#include <Settings.h>
#include <utils.h>
#include "Hardware.h"
#include "I_I2C_Device.h"
#include "IFrameBuffer.h"

static const bool ON = true;
static const bool OFF = false;

// Public constructor sets up SDL, base class tries to set up I2C connection 
Projector::Projector(const I_I2C_Device& i2cDevice, IFrameBuffer& frameBuffer) :
_i2cDevice(i2cDevice),
_frameBuffer(frameBuffer)
{
    // see if we have an I2C connection to the projector
    _canControlViaI2C = (_i2cDevice.Read(PROJECTOR_HW_STATUS_REG) != ERROR_STATUS);

    if (!_canControlViaI2C)
        LOGGER.LogMessage(LOG_INFO, LOG_NO_PROJECTOR_I2C);
    else
    {
        // disable the projector's gamma correction, to provide linear output
        unsigned char disable = PROJECTOR_GAMMA_DISABLE;
        _i2cDevice.Write(PROJECTOR_GAMMA, &disable, 1);
    }

    ShowBlack();
     
    Magick::InitializeMagick("");
}

Projector::~Projector() 
{
    // don't throw exceptions from destructor
    try
    {
        ShowBlack();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

// Sets the image for display but does not actually draw it to the screen
void Projector::SetImage(Magick::Image& image)
{
    _frameBuffer.Attach(image);
    
}

// Display the given image (or _image if given image is NULL).
void Projector::ShowCurrentImage()
{
    _frameBuffer.Draw();
    TurnLEDOn();
}

// Display an all black image.
void Projector::ShowBlack()
{
    TurnLEDOff();
    Magick::Image image(Magick::Geometry(_frameBuffer.Width(),
                                         _frameBuffer.Height()),
                        Magick::ColorMono(false));
    _frameBuffer.Attach(image);
    _frameBuffer.Draw();
}

// Display an all white image.
void Projector::ShowWhite()
{
    Magick::Image image(Magick::Geometry(_frameBuffer.Width(),
                                         _frameBuffer.Height()),
                        Magick::ColorMono(true));
    _frameBuffer.Attach(image);
    _frameBuffer.Draw();
    TurnLEDOn();

}

void Projector::ShowImageFromFile(const std::string& path)
{
    Magick::Image image(path);
    _frameBuffer.Attach(image);
    _frameBuffer.Draw();
    TurnLEDOn();
}

// Turn the projector's LED(s) off.
void Projector::TurnLEDOff()
{
    if (!_canControlViaI2C)
        return;
 
    _i2cDevice.Write(PROJECTOR_LED_ENABLE_REG, PROJECTOR_DISABLE_LEDS);
}

// Set the projector's LED(s) current and turn them on. Set the current every
// time to prevent having to restart the system to observe the effects of
// changing the LED current setting.
void Projector::TurnLEDOn()
{
    if (!_canControlViaI2C)
        return;
 
    // set the LED current, if we have a valid setting value for it
    int current = SETTINGS.GetInt(PROJECTOR_LED_CURRENT);
    
    if (current > 0)
    {
        // Set the PWM polarity.
        // Though the PRO DLPC350 Programmer’s Guide says to set this after 
        // setting the LED currents, it appears to need to be set first.
        // Also, the Programmer’s Guide seems to have the 
        // polarity backwards.
        unsigned char polarity = PROJECTOR_PWM_POLARITY_NORMAL;
        _i2cDevice.Write(PROJECTOR_LED_PWM_POLARITY_REG, &polarity, 1);
        
        unsigned char c = static_cast<unsigned char>(current);

        // use the same value for all three LEDs
        unsigned char buf[3] = {c, c, c};

        _i2cDevice.Write(PROJECTOR_LED_CURRENT_REG, buf, 3);
    }

    _i2cDevice.Write(PROJECTOR_LED_ENABLE_REG, PROJECTOR_ENABLE_LEDS);
}
