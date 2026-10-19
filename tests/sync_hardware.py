"""Opt-in check against ONE lamp, with this computer simulating its UDP peer.
Run: python3 tests/sync_hardware.py http://192.168.0.27
Temporarily changes sync/power/mode; restores original lighting and sync afterwards.
All other updated lamps in the subnet also see group enable/disable during this test.
"""
import copy
import json
import socket
import sys
import time
import urllib.request
base = sys.argv[1].rstrip('/')
host = base.split('://', 1)[1].split(':')[0]
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def api(path, payload=None):
    request = urllib.request.Request(base+path, data=None if payload is None else json.dumps(payload).encode(), headers={'Content-Type':'application/json'})
    with opener.open(request, timeout=4) as response:
        return json.load(response)
def wait_state(predicate):
    deadline=time.monotonic()+4
    while time.monotonic()<deadline:
        state=api('/api/state')
        if predicate(state):return state
        time.sleep(.15)
    raise AssertionError('Lamp did not converge')
original=api('/api/state')
allowed={'power','mode','hue','saturation','effectHue','effectSaturation','effectCustomColor'}
restore={k:v for k,v in original.items() if k in allowed or k.endswith('Brightness') or k.endswith('Speed')}
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
sock.bind(('',4210))
sock.settimeout(.5)
def receive_packet():
    deadline=time.monotonic()+6
    while time.monotonic()<deadline:
        try:data,source=sock.recvfrom(2048)
        except socket.timeout:continue
        if source[0]==host:
            packet=json.loads(data)
            if packet.get('enabled'):return packet
    raise AssertionError('No outgoing UDP broadcast received')
try:
    result=api('/api/sync',{'enabled':True})
    assert result['enabled']
    packet=receive_packet()
    assert packet['state']['mode']==original['mode']
    peer=copy.deepcopy(packet)
    peer['stateCounter']+=1
    peer['stateOwner']=1
    peer['state']={'power':False,'mode':'plasma'}
    sock.sendto(json.dumps(peer).encode(),(host,4210))
    wait_state(lambda s:not s['power'] and s['mode']=='plasma')
    api('/api/state',{'power':True,'mode':'aurora'})
    # Same older peer revision must not undo a newer local HTTP command.
    for _ in range(3):sock.sendto(json.dumps(peer).encode(),(host,4210))
    time.sleep(.25)
    state=api('/api/state');assert state['power'] and state['mode']=='aurora'
    api('/api/sync',{'enabled':False})
    peer['stateCounter']+=100
    sock.sendto(json.dumps(peer).encode(),(host,4210))
    time.sleep(.25)
    state=api('/api/state');assert not state['syncEnabled'] and state['power'] and state['mode']=='aurora'
    print('PASS: physical ESP8266 outgoing broadcast, incoming peer state, stale command suppression, disabled sync suppression')
finally:
    api('/api/state',restore)
    api('/api/sync',{'enabled':bool(original.get('syncEnabled',False))})
    sock.close()
