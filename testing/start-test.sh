#!/bin/sh
tmux new-session -d -s llsim 'python3 link_simulator.py'
tmux split-window -v 'exec ./test-terminal.py -p 52000'
#tmux split-window -h -t 0 -p 80 'gdb ./skylink -ex "run"'
tmux split-window -h -t 0 -p 80 './skylink'
#tmux split-window -h -t 1 'gdb ./skylink -ex "run m 53700 62000"'
tmux split-window -h -t 1 'exec ./skylink m 53700 62000'

tmux split-window -h -t 3 'exec ./test-terminal.py -p 52002'
tmux split-window -h -t 3 'exec ./test-terminal.py -p 52001'
tmux split-window -h -t 5 'exec ./test-terminal.py -p 52003'

tmux split-window -v -t 3 -p 50 'exec ./test-terminal.py -p 62000'
tmux split-window -v -t 5 -p 50 'exec ./test-terminal.py -p 62001'
tmux split-window -v -t 7 -p 50 'exec ./test-terminal.py -p 62002'
tmux split-window -v -t 9 -p 50 'exec ./test-terminal.py -p 62003'
tmux attach-session -t llsim
