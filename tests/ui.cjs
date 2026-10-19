// Run: NODE_PATH=/path/to/node_modules node tests/ui.cjs
const { chromium } = require('playwright');
const http = require('node:http');
const fs = require('node:fs');
const assert = require('node:assert/strict');
const state = (id,name,ip) => ({id,name,ip,connected:true,power:true,mode:'fire',fireBrightness:128,colorBrightness:180,auroraBrightness:160,plasmaBrightness:128,hiphoticBrightness:128,pacificaBrightness:160,hue:30,saturation:255,effectHue:0,effectSaturation:255,effectCustomColor:false,syncEnabled:false,fireSpeed:100,colorSpeed:100,auroraSpeed:100,plasmaSpeed:100,hiphoticSpeed:100,pacificaSpeed:100,fire2012Brightness:128,fire2012Speed:100});
const states = {'':state('a','Гостиная','192.168.1.10'),'192.168.1.11':state('b','Спальня','192.168.1.11')};
const tints={};
let offline=false, latency=0, wifiSaved=null;
let scanCalls=0, scanFails=false, syncFails=false, discoveryHost='192.168.1.11';
const server=http.createServer(async(req,res)=>{
  const u=new URL(req.url,'http://localhost');res.setHeader('Content-Type','application/json');
  if(u.pathname==='/'){res.setHeader('Content-Type','text/html');return res.end(fs.readFileSync('web/index.html'))}
  if(u.pathname==='/api/wifi'){
    if(req.method==='POST'){let body='';for await(const c of req)body+=c;wifiSaved=JSON.parse(body);return res.end('{"ok":true,"restarting":true}')}
    return res.end(JSON.stringify({ssid:'Home Test',connected:true,ip:states[''].ip}));
  }
  if(u.pathname==='/api/wifi/scan'){
    if(scanFails){res.statusCode=503;return res.end('{"error":"Wi-Fi scan failed"}')}
    if(++scanCalls===1){res.statusCode=202;return res.end('{"scanning":true}')}
    return res.end(JSON.stringify({scanning:false,networks:[{ssid:'My Home',rssi:-40,secured:true},{ssid:'Open Cafe',rssi:-65,secured:false},{ssid:'<img src=x>',rssi:-70,secured:true}]}));
  }
  if(u.pathname==='/api/sync'){
    if(syncFails){res.statusCode=503;return res.end('{"error":"Sync unavailable"}')}
    let body='';for await(const c of req)body+=c;const {enabled}=JSON.parse(body);
    for(const state of Object.values(states))state.syncEnabled=enabled;
    return res.end(JSON.stringify({enabled,connected:true}));
  }
  if(u.pathname==='/api/discover')return res.end(JSON.stringify({lamps:[{host:'192.168.1.10',id:'a',name:'Гостиная',self:true},{host:discoveryHost,id:'b',name:'Спальня'},{host:discoveryHost,id:'b',name:'Спальня'}]}));
  const host=u.searchParams.get('host')||'', s=states[host];
  if(!s||(offline&&host)){res.statusCode=504;return res.end('{"error":"Lamp unavailable"}')}
  if(req.method==='POST'){
    let body='';for await(const c of req)body+=c;const patch=JSON.parse(body);
    const mode=patch.mode||s.mode,key=host+':'+mode;
    tints[key]??={effectHue:0,effectSaturation:255,effectCustomColor:false};
    for(const field of ['effectHue','effectSaturation','effectCustomColor'])if(field in patch)tints[key][field]=patch[field];
    Object.assign(s,patch,tints[key]);
    if(s.syncEnabled)for(const other of Object.values(states))if(other!==s)Object.assign(other,patch);
    if(latency)await new Promise(r=>setTimeout(r,latency));
  }
  Object.assign(s,tints[host+':'+s.mode]||{effectHue:0,effectSaturation:255,effectCustomColor:false});
  res.end(JSON.stringify(s));
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
  try{
    const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto('http://127.0.0.1:'+server.address().port);
    await page.waitForFunction(()=>document.querySelector('#title').textContent==='Гостиная');
    await page.waitForFunction(()=>document.querySelector('#ssid').value==='Home Test');
    assert.equal(await page.locator('#buttonConfig').count(),0);
    assert.deepEqual(await page.locator('.modes button').evaluateAll(nodes=>nodes.map(n=>n.id)),['fire','fire2012','color','aurora','plasma','hiphotic','pacifica']);
    assert.equal(await page.locator('#touchPin').count(),0);
    await page.waitForFunction(()=>document.querySelector('#networks').options.length===4);
    assert.ok(scanCalls>=2);
    await page.selectOption('#networks','<img src=x>');assert.equal(await page.inputValue('#ssid'),'<img src=x>');
    assert.equal(await page.locator('#networks img').count(),0);
    scanFails=true;await page.click('#scanWifi');await page.waitForTimeout(100);
    assert.match(await page.textContent('#scanStatus'),/SSID можно ввести вручную/);
    assert.equal(await page.inputValue('#ssid'),'<img src=x>');
    scanFails=false;await page.click('#scanWifi');await page.waitForTimeout(100);
    await page.selectOption('#networks','My Home');assert.equal(await page.inputValue('#ssid'),'My Home');
    await page.fill('#ssid','My Home');await page.fill('#password','short');await page.locator('#wifi button').click();
    assert.equal(wifiSaved,null);assert.match(await page.textContent('#notice'),/минимум 8/);
    await page.fill('#password','home-password');await page.check('#showPassword');
    assert.equal(await page.locator('#password').getAttribute('type'),'text');
    await page.locator('#wifi button').click();await page.waitForTimeout(100);
    assert.deepEqual(wifiSaved,{ssid:'My Home',password:'home-password'});assert.equal(await page.inputValue('#password'),'');
    await page.click('#color');await page.waitForFunction(()=>!document.querySelector('#wheel').hidden);
    await page.locator('#hue').fill('220');await page.locator('#hue').dispatchEvent('input');
    await page.locator('#brightness').fill('65');await page.locator('#brightness').dispatchEvent('input');
    await page.waitForTimeout(400);assert.equal(states[''].hue,220);assert.equal(states[''].colorBrightness,65);
    latency=350;
    await page.locator('#brightness').fill('20');await page.locator('#brightness').dispatchEvent('input');await page.waitForTimeout(220);
    await page.locator('#brightness').fill('65');await page.locator('#brightness').dispatchEvent('input');await page.waitForTimeout(950);
    assert.equal(states[''].colorBrightness,65);assert.equal(await page.inputValue('#brightness'),'65');latency=0;
    await page.click('#fire');await page.waitForTimeout(200);assert.equal(await page.inputValue('#brightness'),'128');
    await page.click('#color');await page.waitForTimeout(200);assert.equal(await page.inputValue('#brightness'),'65');
    for(const [index,mode] of ['aurora','plasma','hiphotic','pacifica','fire2012'].entries()){
      await page.click('#'+mode);await page.waitForTimeout(100);
      assert.equal(states[''].mode,mode);assert.equal(await page.locator('#effectCard').isVisible(),true);
      assert.equal(await page.locator('#wheel').isVisible(),false);
      await page.check('#customColor');await page.waitForTimeout(100);
      assert.equal(await page.locator('#wheel').isVisible(),true);
      await page.locator('#hue').fill(String(90+index*30));await page.locator('#hue').dispatchEvent('input');
      await page.locator('#sat').fill('200');await page.locator('#sat').dispatchEvent('input');
      await page.waitForTimeout(300);
      assert.equal(states[''].effectHue,90+index*30);assert.equal(states[''].effectSaturation,200);
      assert.equal(states[''].hue,220);

      await page.locator('#brightness').fill(String(70+index));await page.locator('#brightness').dispatchEvent('input');
      await page.waitForTimeout(300);assert.equal(states[''][mode+'Brightness'],70+index);
      await page.locator('#speed').fill(String(50+index*25));await page.locator('#speed').dispatchEvent('input');
      await page.waitForTimeout(250);assert.equal(states[''][mode+'Speed'],50+index*25);

    }
    await page.click('#aurora');await page.waitForTimeout(150);assert.equal(await page.inputValue('#brightness'),'70');assert.equal(await page.inputValue('#hue'),'90');assert.equal(await page.inputValue('#speed'),'50');
    await page.uncheck('#customColor');await page.waitForTimeout(150);assert.equal(await page.locator('#wheel').isVisible(),false);
    await page.check('#customColor');await page.waitForTimeout(150);assert.equal(await page.inputValue('#hue'),'90');
    await page.reload();await page.waitForFunction(()=>document.querySelector('#aurora').classList.contains('active'));
    assert.equal(await page.inputValue('#brightness'),'70');
    await page.waitForFunction(()=>document.querySelector('#count').textContent==='2');
    assert.equal(await page.locator('#lamps .lamp').count(),2);
    assert.match(await page.textContent('#lamps'),/Спальня/);
    syncFails=true;await page.check('#syncAll');await page.waitForTimeout(150);
    assert.equal(await page.isChecked('#syncAll'),false);assert.match(await page.textContent('#notice'),/Не удалось изменить синхронизацию/);
    syncFails=false;await page.check('#syncAll');await page.waitForTimeout(150);
    assert.equal(states[''].syncEnabled,true);assert.equal(states['192.168.1.11'].syncEnabled,true);
    await page.click('#hiphotic');await page.waitForTimeout(150);assert.equal(states['192.168.1.11'].mode,'hiphotic');
    await page.click('#power');await page.waitForTimeout(150);assert.equal(states['192.168.1.11'].power,false);
    await page.click('#power');await page.waitForTimeout(150);
    await page.reload();await page.waitForFunction(()=>document.querySelector('#syncAll').checked);
    await page.uncheck('#syncAll');await page.waitForTimeout(150);
    assert.equal(states['192.168.1.11'].syncEnabled,false);
    await page.click('#color');await page.waitForTimeout(150);assert.equal(states['192.168.1.11'].mode,'hiphotic');
    await page.click('#all');await page.click('#fire');await page.waitForTimeout(300);
    assert.equal(states[''].mode,'fire');assert.equal(states['192.168.1.11'].mode,'fire');
    await page.locator('#brightness').fill('99');await page.locator('#brightness').dispatchEvent('input');await page.waitForTimeout(400);
    assert.equal(states[''].fireBrightness,99);assert.equal(states['192.168.1.11'].fireBrightness,99);
    for(const mode of ['aurora','plasma','hiphotic','pacifica','fire2012']){
      await page.click('#'+mode);await page.waitForTimeout(150);
      assert.equal(states[''].mode,mode);assert.equal(states['192.168.1.11'].mode,mode);
    }
    await page.check('#customColor');
    await page.locator('#hue').fill('310');await page.locator('#hue').dispatchEvent('input');await page.waitForTimeout(300);
    assert.equal(states[''].effectHue,310);assert.equal(states['192.168.1.11'].effectHue,310);
    await page.locator('#speed').fill('225');await page.locator('#speed').dispatchEvent('input');await page.waitForTimeout(300);assert.equal(states['192.168.1.11'].fire2012Speed,225);
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
    await page.screenshot({path:'/tmp/fire-lamp-new-effects-mobile.png',fullPage:true});
    offline=true;await page.click('#power');await page.waitForTimeout(400);
    assert.equal(states[''].power,false);assert.match(await page.textContent('#notice'),/Не удалось применить/);
    offline=false;await page.click('#all');await page.click('#lampSettings summary');
    await page.fill('#name','<img src=x>');await page.locator('#rename button').click();await page.waitForTimeout(300);
    assert.equal(await page.textContent('#title'),'<img src=x>');assert.equal(await page.locator('#title img').count(),0);
    await page.setViewportSize({width:390,height:844});await page.screenshot({path:'/tmp/fire-lamp-mobile.png',fullPage:true});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
    await page.reload();await page.waitForFunction(()=>document.querySelector('#count').textContent==='2');
    // The same stable ID moves to another address; timer polling must merge it.
    states['192.168.1.12']={...states['192.168.1.11'],ip:'192.168.1.12',name:'Новая спальня'};
    delete states['192.168.1.11'];discoveryHost='192.168.1.12';
    await page.waitForFunction(()=>document.querySelector('#lamps').textContent.includes('192.168.1.12'),{},{timeout:12000});
    await page.waitForFunction(()=>document.querySelector('#lamps').textContent.includes('Новая спальня'));
    assert.equal(await page.locator('#lamps .lamp').count(),2);
    assert.doesNotMatch(await page.textContent('#lamps'),/192\.168\.1\.11/);
    assert.deepEqual(errors,[]);console.log('PASS: automatic lamp discovery without manual actions, self/peer deduplication, DHCP address change and rename, Wi-Fi setup, password validation, asynchronous SSID scan, scan failure recovery, hidden sensor settings, seven effects, independent speed and group speed, per-effect tint and saturation, original palette restore, group colors, persistent sync checkbox, toggle failure recovery, independent effect brightness, effect reload, group effects, independent brightness, hue, multi-lamp commands, partial failure, safe names, mobile layout, persisted registry');
  }finally{await browser.close();server.close()}
})().catch(e=>{console.error(e);server.close();process.exitCode=1});
