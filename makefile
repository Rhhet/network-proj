FLAGS = -Wall -pedantic -std=gnu99 -pthread
CC = gcc
EXEPATH = ./exe/
SRCPATH = ./src/
EXEC = $(EXEPATH)router
EXE = router
FULLEXC = exe/

all: $(EXE)

router: router.o console.o test_forwarding.o
	$(CC) $(FLAGS) $(EXEPATH)*.o -o $@

# '%' matches filename
# $@  for the pattern-matched target
# $<  for the pattern-matched dependency
%.o: $(SRCPATH)%.c
	$(CC) $(FLAGS) -o $(EXEPATH)$@ -c $<

test_forwarding: router
	for r in 1 2 3 4 ; do \
		xterm -title "R $$r" -e ./router $$r --test-forwarding & \
	done

test_topo1: router
	for r in 1 2 3 ; do \
		xterm -title "R $$r" -e ./router $$r topos/t1.txt & \
	done

test_topo2: router
	for r in 1 2 3 4 5 ; do \
		xterm -title "R $$r" -e ./router $$r topos/t2.txt & \
	done

test_topo3: router
	for r in 1 2 3 4 ; do \
		xterm -title "R $$r" -e ./router $$r topos/t3.txt & \
	done

test_topo4: router
	for r in 1 2 3 4 5 6 7 ; do \
		xterm -title "R $$r" -e ./router $$r topos/t4.txt & \
	done

test_topo5: router
	for r in 1 2 3 4 5 ; do \
		xterm -title "R $$r" -e ./router $$r topos/t5.txt & \
	done

kill_test:
	for p in `pgrep router`; do kill $$p; done

clean: kill_test
	rm -f $(EXEC)
	rm -f $(EXEPATH)*.o
	rm -f log/*

# these targets are used along with VScode tasks to compile the source files
# replace 'Source Code Pro' by any other font

launchTF:
	for r in 1 2 3 4 ; do \
		xterm -title "R $$r" -fa 'Source Code Pro' -bg Grey23 -e ./router $$r --test-forwarding & \
	done

launchT1:
	for r in 1 2 3 ; do \
		xterm -title "R $$r" -fa 'Source Code Pro' -bg Grey23 -e ./router $$r topos/t1.txt & \
	done

launchT2:
	for r in 1 2 3 4 5; do \
		xterm -title "R $$r" -fa 'Source Code Pro' -bg Grey23 -e ./router $$r topos/t2.txt & \
	done

launchT3:
	for r in 1 2 3 4; do \
		xterm -title "R $$r" -fa 'Source Code Pro' -bg Grey23 -e ./router $$r topos/t3.txt & \
	done

launchT4:
	for r in 1 2 3 4 5 6 7; do \
		xterm -title "R $$r" -fa 'Source Code Pro' -bg Grey23 -e ./router $$r topos/t4.txt & \
	done

launchT5:
	for r in 1 2 3 4 5; do \
		xterm -title "R $$r" -fa 'Source Code Pro' -bg Grey23 -e ./router $$r topos/t5.txt & \
	done

clean_logs:
	rm -f log/*