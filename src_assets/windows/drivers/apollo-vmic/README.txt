Apollo Virtual Microphone driver payload
=======================================

This directory contains Apollo's currently bundled Windows virtual microphone
driver payload, based on the signed MIT-licensed "Virtual Audio Driver by MTT"
package:

- VirtualAudioDriver.inf
- virtualaudiodriver.cat
- VirtualAudioDriver.sys

Apollo installs the package as an integrated dependency and targets the
"Virtual Audio Driver by MTT" render endpoint for redirected client microphone
audio. Windows applications should then select the paired
"Virtual Mic Driver by MTT" capture endpoint on the host.

The helper scripts in this directory call Apollo's installer utility to create
and remove the root-enumerated device node automatically.
