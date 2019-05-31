# Change the UI color of your Nintendo Switch controller!
<h4>Cut up version of hidtest by shinyquagsire29. Changes the color of your JoyCon/Pro Controller in the Nintendo Switch's UI.</h4>

This is meant to be for users on Linux who cannot use jc_toolkit and just want to change the color in a kinda simple-ish way.

Please keep in mind that while this will almost certainly not get you banned from online play, it is considered a cosmetic modification by Nintendo and will void the warranty on your controller.

_The software is provided "as is", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose and noninfringement. In no event shall the
authors or copyright holders be liable for any claim, damages or other
liability, whether in an action of contract, tort or otherwise, arising from,
out of or in connection with the software or the use or other dealings in the
software._

---
__Instructions__
1. Open up the file ns_colorchange.cpp
2. Change applicable values.
	+ body[] and sticks[] array values. (instructions are above them)
	+ bluetooth boolean (true or false depending on how your controller will be connected.
3. Compile the script. You will need to recompile it every time you want to change any values.
	+ `g++ -o recolor -lhidapi-hidraw ns_colorchange.cpp`
---

### TODO
I probably won't be adding much to the project personally, but if you have a lazy afternoon and want to do a few of these, please don't hesitate to make a PR!
+ Remove more leftover code from hidtest.
+ Add support for handle colors.
+ Make the script prompt for colors and Bluetooth status when it runs so the script doesn't have to be recompiled every time you want to change the color.
+ Add a license once shinyquagsire29 adds one to HID Joy-Con Whispering.


This project has only been extensively tested on Linux, and requires hidapi as a prerequisite.
