#pragma once

// The device's web page, on port 80 of both networks: on the setup network it's a captive portal that
// asks for Wi-Fi; on the user's network it's the settings page (http://brickman-xxxx.local/).
// Saving anything restarts the device, so every module simply reads settings at startup.
namespace web {

void begin();

}  // namespace web
