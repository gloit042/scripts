#!/usr/bin/env python3
import atexit
import json
import argparse
import subprocess
import os
import stat
import time
import signal

MODE = stat.S_IROTH | stat.S_IWOTH
pairs = []
group_pairs = []

def cleanup():
  for (p, of) in pairs:
    p.kill()
    if of != None:
      of.close()
  for (p, of) in group_pairs:
    os.killpg(os.getpgid(p.pid), signal.SIGTERM)
    if of != None:
      of.close()

atexit.register(cleanup)


def record(conf, arg):
    last_time = arg.last_time
    interval = arg.interval
    log_dir = "records-"
    log_dir += time.strftime('%Y%m%d-%H%M%S', time.localtime(time.time()))
    os.mkdir(log_dir)
    if conf.get('disk') != None:
        disk_conf = conf['disk']
        assert(isinstance(disk_conf['devs'], list))
        if disk_conf.get('log') == None:
            disk_conf['log'] = 'disk.log'
        else:
            assert(isinstance(disk_conf['log'], str))
        of = open(log_dir + '/' + disk_conf['log'], 'w')
        mode = os.fstat(of.fileno()).st_mode
        mode = mode | MODE
        os.fchmod(of.fileno(), mode)
        command = ['iostat', '-t', '-d', str(interval), '-y']
        command.extend(disk_conf['devs'])
        p = subprocess.Popen(command, stdout=of)
        pairs.append((p,of))

    if conf.get('network') != None:
        net_conf = conf['network']
        assert(isinstance(net_conf['dsts'], list))
        prefix = 'network.log'
        if net_conf.get('log-prefix') != None:
            assert(isinstance(net_conf['log-prefix'], str))
            prefix = net_conf['log-prefix']

        for n, dst in enumerate(net_conf['dsts']):
            of = open(log_dir + '/' + prefix + '.' + str(n), 'w')
            mode = os.fstat(of.fileno()).st_mode
            mode = mode | MODE
            os.fchmod(of.fileno(), mode)
            command = ['tcpdump -i ib0 -l -e -n dst %s | ./tools/netbps.pl -t %d' % (dst, interval)]
            p = subprocess.Popen(command, shell=True, stdout=of, preexec_fn=os.setsid)
            group_pairs.append((p,of))

    if conf.get('mem') != None:
        mem_conf = conf['mem']
        logfile = 'mem.log'
        if mem_conf.get('log') != None:
            assert(isinstance(mem_conf['log'], str))
            log = mem_conf['log']
        
        of = open(log_dir + '/' + logfile, 'w')
        mode = os.fstat(of.fileno()).st_mode
        mode = mode | MODE
        os.fchmod(of.fileno(), mode)
        command = ['tools/record.sh', 'mem', str(interval)]
        p = subprocess.Popen(command, stdout=of)
        pairs.append((p,of))

    if conf.get('cpu') != None:
        cpu_conf = conf['cpu']
        logfile = 'cpu.log'
        if cpu_conf.get('log') != None:
            assert(isinstance(cpu_conf['log'], str))
            log = cpu_conf['log']
        
        of = open(log_dir + '/' + logfile, 'w')
        mode = os.fstat(of.fileno()).st_mode
        mode = mode | MODE
        os.fchmod(of.fileno(), mode)
        command = ['tools/record.sh', 'cpu', str(interval)]
        p = subprocess.Popen(command, stdout=of)
        pairs.append((p,of))


    time.sleep(last_time)
    for (p, of) in pairs:
        p.kill()
        if of != None:
          of.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='utility to record machine status')
    parser.add_argument('-c', '--config', metavar='CONFIG', dest='config',
                        help='choose config file(default: ./conf.json)', default='conf.json')
    parser.add_argument('-t', '--time', metavar='SECONDS', type=int, dest='last_time',
                        help='recording window(seconds))', required=True)
    parser.add_argument('-i', '--interval', metavar='SECONDS', type=int, dest='interval',
                        help='recording interval(seconds))', default=1)
    os.putenv('S_TIME_FORMAT', 'ISO')
    arg = parser.parse_args()
    if os.access(arg.config, os.R_OK):
        with open(arg.config, 'r') as f:
            conf = json.load(f)
            record(conf, arg)
    else:
        print('can\'t read from config path:', arg.config)
        parser.print_help();

