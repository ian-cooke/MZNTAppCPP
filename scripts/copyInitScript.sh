#ssh-keygen -f "/home/chris/.ssh/known_hosts" -R 128.138.253.168
ssh-keygen -f "/home/chris/.ssh/known_hosts" -R $1
scp ./mznt-logger root@$1:/etc/init.d/mznt-logger
scp ./config.txt root@$1:/mnt/
