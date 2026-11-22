#!/usr/bin/env python3
"""Isolated radiod -> real dynamic tune -> checkpoint -> process restart -> RTP test.

Run ONLY inside an independent Linux network namespace with loopback UP.
Never run on the host network: multicast addresses/ports are intentionally real.
Uses a compiled sig_gen.so, not physical SDR hardware or installed radiod.
"""
import argparse
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time

GROUP = "239.254.71.70"
DATA = "239.254.71.71"
SSRC1, SSRC2 = 100001, 100002

def ensure_network_isolation():
    if not Path("/proc/self/ns/net").exists():
        raise RuntimeError("Linux network namespaces unavailable")
    if os.stat("/proc/self/ns/net").st_ino == os.stat("/proc/1/ns/net").st_ino:
        raise RuntimeError("Refusing host (PID 1) network namespace")
    # A user namespace is expected too; the wrapper enters both.
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.settimeout(0.4)
        s.bind(("127.0.0.1",0))

def wait_for(predicate, seconds, description):
    end = time.monotonic()+seconds
    while time.monotonic()<end:
        if predicate():
            return
        time.sleep(.2)
    raise RuntimeError("Timed out: "+description)

def read_file(p):
    return p.read_text(errors="replace") if p.exists() else ""

def launch(radiod,config,checkpoint,log,restore=False):
    args=[str(radiod),"--dynamic-state-file",str(checkpoint)]
    if restore:
        args.append("--restore-dynamic-state")
    args.append(str(config))
    f=open(log,"w")
    try:
        p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
    finally:
        f.close()
    return p

def stop(p):
    if p.poll() is not None:
        return
    p.terminate()
    try:
        p.wait(timeout=4)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait(timeout=3)

def started(p,log):
    if p.poll() is not None:
        raise RuntimeError("RADIOD exited early:\n"+read_file(log)[-4000:])
    s=read_file(log)
    return ("0 static demodulators started" in s
            and "can't listen for commands" not in s
            and "signal generator" not in s.lower() or False)

def wait_control_ready(p,log):
    def ready():
        if p.poll() is not None:
            raise RuntimeError("Test RADIOD exited:\n"+read_file(log)[-4000:])
        t=read_file(log)
        if "can't listen for commands" in t:
            raise RuntimeError("Test RADIOD did not open control socket:\n"+t[-2000:])
        return "0 static demodulators started" in t
    wait_for(ready,10,"RADIOD configuration startup")
    time.sleep(.6)

def tune(binary,ssrc,frequency,mode,samprate,log):
    cmd=[str(binary),"-q","-r",GROUP,"-s",str(ssrc),
         "-f",str(frequency),"-m",mode,"-R",str(samprate),
         "-c","1","-e","s16be","-t","8000",
         "-D",DATA,"-i","lo"]
    with open(log,"w") as fd:
        try:
            r=subprocess.run(cmd,stdout=fd,stderr=subprocess.STDOUT,timeout=12)
        except subprocess.TimeoutExpired:
            raise RuntimeError("tune timed out for SSRC "+str(ssrc)+"\n"+read_file(log)[-1500:])
    if r.returncode!=0:
        raise RuntimeError("tune failed for SSRC "+str(ssrc)+"\n"+read_file(log)[-1500:])

def checkpoint_records(checkpoint):
    text=read_file(checkpoint)
    lines=[x.strip() for x in text.splitlines() if x.strip() and not x.startswith("#")]
    if not lines or lines[0]!="KA9Q-RADIOD-DYNAMIC-STATE 1":
        return []
    return lines[1:]

def get_stream_ssrcs(seconds=6):
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM,socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        sock.bind(("",5004))
        sock.setsockopt(socket.IPPROTO_IP,socket.IP_ADD_MEMBERSHIP,
                        struct.pack("=4s4s",socket.inet_aton(DATA),socket.inet_aton("127.0.0.1")))
        sock.settimeout(.3)
        end=time.monotonic()+seconds
        seen=set()
        while time.monotonic()<end:
            try:
                b,_=sock.recvfrom(65536)
            except socket.timeout:
                continue
            if len(b)>=12 and b[0]>>6==2:
                seen.add(struct.unpack("!I",b[8:12])[0])
                if {SSRC1,SSRC2}<=seen:
                    break
        return seen
    finally:
        sock.close()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--build",required=True,type=Path)
    args=ap.parse_args()
    ensure_network_isolation()
    build=args.build.resolve()
    radiod=build/"src/radiod"
    tune_bin=build/"src/tune"
    driver=build/"src/sig_gen.so"
    presets=build/"share/presets.conf"
    for p in (radiod,tune_bin,driver,presets):
        if not p.is_file():
            raise RuntimeError("Missing test artifact "+str(p))
    run=Path(tempfile.mkdtemp(prefix="ka9q-dynamic-process-"))
    checkpoint=run/"channels.chk"
    config=run/"test.conf"
    config.write_text(
        "[global]\n"
        "description = dynamic-state-isolated-test\n"
        "hardware = sig_gen\n"
        "status = "+GROUP+"\n"
        "data = "+DATA+"\n"
        "mode = fm\n"
        "advertise = no\n"
        "static = no\n"
        "dns = no\n"
        "ttl = 0\n"
        "fft-plan-level = estimate\n"
        "fft-threads = 1\n"
        "fft-internal-threads = 1\n"
        "blocktime = 0.02\n"
        "presets-file = "+str(presets)+"\n"
        "[sig_gen]\n"
        "device = sig_gen\n"
        "library = "+str(driver)+"\n"
        "samprate = 480k\n"
        "real = yes\n"
        "carrier = 100k\n"
        "amplitude = -30\n"
        "modulation = FM\n")
    print("Artifacts/logs:",run,flush=True)
    first=launch(radiod,config,checkpoint,run/"first.log")
    try:
        wait_control_ready(first,run/"first.log")
        tune(tune_bin,SSRC1,"100k","fm","24k",run/"tune1.log")
        tune(tune_bin,SSRC2,"120k","fm","48k",run/"tune2.log")
        def two_records():
            if first.poll() is not None:
                raise RuntimeError("RADIOD exited while creating channels:\n"+read_file(run/"first.log")[-2000:])
            lines=checkpoint_records(checkpoint)
            return len(lines)==2 and any(x.startswith(str(SSRC1)+" ") for x in lines) and any(x.startswith(str(SSRC2)+" ") for x in lines)
        wait_for(two_records,10,"two dynamically checkpointed SSRCs")
        checkpoint_before=checkpoint.read_bytes()
        print("PASS: two dynamic receiver records persisted by real RADIOD",flush=True)
    finally:
        stop(first)
    second=launch(radiod,config,checkpoint,run/"restore.log",True)
    try:
        def restored():
            if second.poll() is not None:
                raise RuntimeError("Restored RADIOD exited:\n"+read_file(run/"restore.log")[-4000:])
            return "dynamic-state restore complete: restored=2 " in read_file(run/"restore.log")
        wait_for(restored,12,"real process restart and restore=2")
        print("PASS: restarted RADIOD restored two dynamic receivers",flush=True)
        assert checkpoint.read_bytes()==checkpoint_before,"Unexpected checkpoint mutation during restore"
        print("PASS: original checkpoint preserved on restore",flush=True)
        seen=get_stream_ssrcs(7)
        if not {SSRC1,SSRC2}<=seen:
            raise RuntimeError("Missing post-restore RTP streams: received SSRCs "+repr(sorted(seen)))
        print("PASS: RTP resumes for both SSRCs after restart",flush=True)
    finally:
        stop(second)
    print("PASS: isolated synthetic-SDR process restart recovery",flush=True)

if __name__=="__main__":
    try:
        main()
    except Exception as exc:
        print("FAIL:",exc,file=sys.stderr,flush=True)
        sys.exit(1)
