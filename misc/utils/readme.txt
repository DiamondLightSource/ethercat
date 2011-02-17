UNIX socket messaging and msgQLib

ps -eLo cmd,policy,rtprio,pcpu | grep testq

server

socket
fixed N connection thread pool (1 reader, 1 writer)
worker thread message queues
timer threads (1KHz realtime, 1s normal)

Can't use EPICS due to various messageQueue probs, but use same API.

