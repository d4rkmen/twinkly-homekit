# Twinkly HomeKit Hub for Mongoose OS

Add your [Twinkly Smart Lights](http://twinkly.com) to HomeKit smarthome infrastructure. Use `Siri` voice control and all the scripting potential from `Apple`

![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/adding.gif)

## Supported platforms

* `ESP8266`
* `ESP32`

## Dependencies 

* [Twinkly library](https://github.com/d4rkmen/twinkly) - `Twinkly` Mongoose OS library
* [ARP library](https://github.com/d4rkmen/arp) - `ARP` scan implementation
* [HomeKit ADK library](https://github.com/mongoose-os-libs/homekit-adk) which is port of the official [Apple HomeKit ADK](https://github.com/Apple/HomeKitADK/)
* [WiFi Setup library](https://github.com/d4rkmen/wifi-setup) - Web-based WiFi setup captive portal

## Adding Twinkly device to a hub

Type the accessory IP address in a browser, select desired device and press `Add`

![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/tw-adding.gif)

## Removing Twinkly device from a hub

Type the accessory IP address in a browser, select desired device and press `❌`

![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/tw-removing.gif)

## Manual ON / OFF

Manual control is supported by Web GUI. This way You can another emergency control channel.

![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/onoff.gif)

## HomeKit accessory reset

Type the accessory IP address in a browser and press `HAP reset`

## Accessory design

Every `Twinkly` device added represent itself as a `LightBulb` HomeKit service in the accessory

![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/tw-services.png)

## Identification

Build in LED blinks during the identification

## LED indication

* LED blink on `twinkly` library init
* LED blink on `twinkly` device changes
* LED blink on `HAP` going to update `twinkly` device
* LED blinking continous on connection lost / (re)connecting

## Setup

Using the [Mongoose OS](http://mongoose-os.com) framework:

```
$ mos build
$ mos flash
```

## WiFi settings

Connect WiFi access point name `TWH-XXXX` password `twinklyhub`, select home network and save credentials

![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/wifi-setup.gif) ![](https://github.com/d4rkmen/twinkly-homekit/blob/master/docs/twh.png)

## Factory reset

Hold a button for factory reset. This will remove WiFi settings, HAP server status, Twinkly devices list.

Configuration:

```yml
  - ["pins.button", "i", -1, {title: "Button GPIO pin"}]
  - ["pins.button_hold_ms", "i", 5000, {title: "Button hold time for reset"}]
  - ["pins.button_pull_up", "b", true, {title: "Button pull up or down"}]
```

## Copyrights

 * [d4rkmen](https://github.com/d4rkmen)
 * [Deomid "rojer" Ryabkov](https://github.com/rojer)
 * The HomeKit ADK Contributors