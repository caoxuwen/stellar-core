#! /bin/bash
cd /home/ubuntu/stellar-core
sleep 30
/usr/local/bin/stellar-core --forcescp --conf cfgs/stellar-core-e0.cfg
/usr/local/bin/stellar-core stellar-core --conf cfgs/stellar-core-e0.cfg
echo "stellar service started"
