#!/bin/bash
echo ""
echo ""
echo " * ODROID-JIG service install * "
echo ""
echo ""

systemctl disable odroid-jig.service && sync

cp ./odroid-jig.service /etc/systemd/system/ && sync

systemctl enable odroid-jig.service && sync

systemctl restart odroid-jig.service && sync
