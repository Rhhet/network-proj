# NETWORK PROJECT

*NOTE*: The project is also available on [github](https://github.com/Rhhet/network-proj).

## Contents

### Folders

- **src** (contains source files *console.c, router.c* and *test_forwarding.c* and respective headers + *packet.h*);

- exe (contains object files when using `test_topoX` targets from the *makefile*);

- docs (contain the subject pdf);
  
- log (logging files created when running the app);

- topos (all the network topologies files);

- sockets (the Berkeley client/server sockets examples).

---

### Running the app

The working directory is **src** (meaning `.` is `routing/src/` and `..` is `routing/`). The terminal should be opened in the root folder **routing**.

- Using only makefile: run the targets `test_topoX` (X from 1 to 5)

- Using IDE: compile with `gcc -pthread -o ../router router.c console.c test_forwarding.c`.
then use the `launchTX` (with X in 1 .. 5) targets from the makefile.

---

### Comments on the code

#### Functions

The function

```c
void *process_input_packets(void *args)
```

when processing a control packet (type `CTRL`), has to recover the overlay address of the source. It is done by calling an intermadiate function

```c
static void overlay_addr_from_nt(const neighbors_table_t *nt, node_id_t id,overlay_addr_t *addr)
```

which calculates this address knowing the neighbors table and putting it in `addr`.
Another way to recover this address directly from the call of `recvfrom()` is also given in comments.

---

It is still possible to run the program without using the *split-horizon* method (part 4.4 (a)) by specifying `-DSPLIT_HRZ` as option when compiling the program. This will enable the function

```c
void build_dv_packet(packet_ctrl_t *p, routing_table_t *rt)
```

instead of using the *split-horizon* method function

```c
void build_dv_specific(packet_ctrl_t *p, routing_table_t *rt, node_id_t neigh)
```

---

The function

```c
void remove_obsolete_entries(routing_table_t *rt) {
```

has been coded so that an entry is effectively removed from the table (running `ipr` in a router will not print removed routes). It has also been updated for part 4.4 so that routes with metric exceeding `MAX_METRIC=16` will be removed too.

#### Bugs and Remarks

- When an isolated router (like *R5* in the topology *t2*) looses it unique neighboor (*R4* for *R5* in *t2*), the process will then stop abruptly after 10 secs without even logging the error or display it. This won't affect other routers.
This is more likely to be caused by the `sendto` primitive that may send a `SIGPIPE` signal (according to the documentation) to the process because no server is acutally connected. But this signal should interrupt the `sleep` call below which is not the case; the process terminates right after the `sleep` call.

- Sometimes routes takes 2 broadcast periods (~20 secs) to update. This won't cause any issue however.

- The traceroute tests often display the same times (~0.001s) for each hop. Not sure if intended.

---

##### -- Grandpierre Teri --
