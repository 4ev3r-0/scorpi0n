[jay@jay scorpi0n]$ ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
[jay@jay scorpi0n]$ sudo dmesg | tail -10
[28001.010411] usb 1-2: New USB device found, idVendor=1a86, idProduct=55d3, bcdDevice= 4.45
[28001.010424] usb 1-2: New USB device strings: Mfr=0, Product=2, SerialNumber=3
[28001.010430] usb 1-2: Product: USB Single Serial
[28001.010435] usb 1-2: SerialNumber: 5C4C094575
[28059.762740] usb 1-2: USB disconnect, device number 8
[28101.595863] usb 1-2: new full-speed USB device number 9 using xhci_hcd
[28101.721350] usb 1-2: New USB device found, idVendor=1a86, idProduct=55d3, bcdDevice= 4.45
[28101.721365] usb 1-2: New USB device strings: Mfr=0, Product=2, SerialNumber=3
[28101.721371] usb 1-2: Product: USB Single Serial
[28101.721377] usb 1-2: SerialNumber: 5C4C094575
