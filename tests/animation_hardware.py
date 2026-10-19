"""Opt-in ESP8266 check; temporarily changes light/sync, then restores settings.
Run: python3 tests/animation_hardware.py http://IP
Diagnostics inspect logical rendered RGB values, not physical LED light output.
"""
import json,sys,time,urllib.request
base=sys.argv[1].rstrip('/')
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def api(path,payload=None):
    req=urllib.request.Request(base+path,data=None if payload is None else json.dumps(payload).encode(),headers={'Content-Type':'application/json'})
    with opener.open(req,timeout=5) as response:return json.load(response)
original=api('/api/state')
allowed={'power','mode','hue','saturation','effectHue','effectSaturation','effectCustomColor'}
restore={k:v for k,v in original.items() if k in allowed or k.endswith('Brightness') or k.endswith('Speed')}
try:
    api('/api/sync',{'enabled':False})
    api('/api/state',{'power':True,'mode':'color','colorBrightness':80,'hue':0,'saturation':255})
    time.sleep(.85)
    energy=api('/api/diagnostics')['frameEnergy'];assert energy>0
    api('/api/state',{'power':False})
    samples=[]
    for i in range(9):
        samples.append(api('/api/diagnostics')['frameEnergy'])
        time.sleep(.1)
    assert samples[0]>0 and samples[-1]==0,samples
    assert len(set(samples))>=4,samples
    assert all(a>=b for a,b in zip(samples,samples[1:])),samples
    api('/api/state',{'power':True});time.sleep(.85)
    assert api('/api/diagnostics')['frameEnergy']>0
    # Reversing a running fade restarts from the actual visible frame.
    api('/api/state',{'power':False});time.sleep(.2)
    before=api('/api/diagnostics')['frameEnergy']
    api('/api/state',{'power':True})
    after=api('/api/diagnostics')['frameEnergy']
    assert abs(after-before)<energy*.2,(before,after)
    for percent in [25,300]:
        api('/api/state',{'power':True,'mode':'plasma','plasmaSpeed':percent})
        a=api('/api/diagnostics');started=time.monotonic();time.sleep(.5)
        b=api('/api/diagnostics');elapsed=(time.monotonic()-started)*1000
        ratio=(b['animationMs']-a['animationMs'])/elapsed
        assert percent/100*.7<ratio<percent/100*1.3,(percent,ratio)
    for mode in ['fire2012']:
        state=api('/api/state',{'mode':mode,mode+'Speed':175,'power':True})
        assert state['mode']==mode and state[mode+'Speed']==175
        time.sleep(.9)
        assert api('/api/diagnostics')['frameEnergy']>0,mode
    print('PASS: real ESP8266 eased off/on, interrupted fade continuity, 25%/300% clocks, rendered Fire2012')
finally:
    api('/api/state',restore)
    api('/api/sync',{'enabled':bool(original.get('syncEnabled',False))})
