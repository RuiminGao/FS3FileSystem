# FS3 Filesystem (Version 3.0) - Fall 2021

<p>A user-space device driver for a filesystem building on top of a virtual disk drive.</p>
<p>The filesystem uses LRU cache and connects to disk controller over the network.</p>
  
## Run
<p>To compile, run <code>make</code>.</p>
<p>For server side, run <code>./fs3_server [-v -l fs3_server_log.txt]</code>.</p>
<p>For client side, run <code>./fs3_client [-v -l fs3_client_log.txt] WORKLOAD_FILE</code>.</p>
