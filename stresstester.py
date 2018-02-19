from bitcoinrpc.authproxy import AuthServiceProxy
import subprocess
import json
import time
import random
import string
import psutil

blocks_to_gen = 10
name_per_block = 100
TEST_SEND=False
CHECK_MEM_USAGE=True

lbrycrd_rpc_user = 'test'
lbrycrd_rpc_pw = 'test'
lbrycrd_rpc_ip = '127.0.0.1'
lbrycrd_rpc_port = '8332'


def run_command(cmd):
    """given shell command, returns communication tuple of stdout and stderr"""
    p=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return p.communicate()

if CHECK_MEM_USAGE:
    lbrycrd_pid,err = run_command("pgrep lbrycrdd")

lbrycrd = AuthServiceProxy("http://{}:{}@{}:{}".format(lbrycrd_rpc_user, lbrycrd_rpc_pw,
                                                        lbrycrd_rpc_ip, lbrycrd_rpc_port)
                          ,timeout=60
                            )

time.sleep(15)
lbrycrd.generate(400)
print("Generated initial blocks")
random.seed(9001)

for i in range(0, blocks_to_gen):
    for j in range(0, name_per_block):
        if not TEST_SEND:
            random_len = 100
            random_name = ''.join([random.choice(string.ascii_letters + string.digits) for n in xrange(random_len)])
            lbrycrd.claimname(random_name, 'val', 0.001)
        else:
            address = lbrycrd.getnewaddress()
            lbrycrd.sendtoaddress(address,0.001)
    start_time = time.time()
    lbrycrd.generate(1)
    print("Generated block {}".format(i))
    if CHECK_MEM_USAGE:
        process = psutil.Process(int(lbrycrd_pid))
        mem_used = process.memory_info()[0]
        print("Time to generate:{}, names:{}, mem_used:{} megabytes".format(time.time() - start_time, name_per_block*(i+1), mem_used/1000000))
