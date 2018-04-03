#ssh-keygen -f "/home/chris/.ssh/known_hosts" -R 128.138.253.168
scp ./logger-init2 root@128.138.253.168:/etc/init.d/logger-init
