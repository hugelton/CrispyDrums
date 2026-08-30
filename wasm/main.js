let ctx=null;
let renderHit=null;
let outPtr=0;
let outCapacity=0;
let audioReady=false;

window.Module=window.Module||{};

function createAudio(){
  if(!ctx) ctx=new (window.AudioContext||window.webkitAudioContext)();
  return ctx;
}

async function unlockAudio(){
  const audio=createAudio();
  try{
    if(audio.state==='suspended') await audio.resume();
    const osc=audio.createOscillator();
    const gain=audio.createGain();
    gain.gain.value=0.00001;
    osc.frequency.value=440;
    osc.connect(gain);
    gain.connect(audio.destination);
    osc.start();
    osc.stop(audio.currentTime+0.02);
    audioReady=audio.state==='running';
  }catch(e){
    console.error(e);
    audioReady=false;
  }
  if(audioReady){
    const b=document.getElementById('audioUnlock');
    if(b) b.classList.add('hidden');
  }
  return audioReady;
}

function heapF32(){
  if(Module.HEAPF32) return Module.HEAPF32;
  if(Module.wasmMemory) return new Float32Array(Module.wasmMemory.buffer);
  return null;
}

function ensureBuffer(frames){
  if(frames<=outCapacity) return;
  if(outPtr) Module._free(outPtr);
  outCapacity=frames;
  outPtr=Module._malloc(frames*4);
}

function playVoiceNow(index,pad){
  if(!audioReady||!ctx||ctx.state!=='running'||!renderHit) return;
  const sr=ctx.sampleRate;
  const frames=Math.ceil(sr*0.8);
  ensureBuffer(frames);
  const written=renderHit(index,outPtr,frames,sr);
  const heap=heapF32();
  if(!heap||written<=0) return;
  const start=outPtr>>2;
  const mono=new Float32Array(written);
  mono.set(heap.subarray(start,start+written));
  const b=ctx.createBuffer(1,written,sr);
  b.copyToChannel(mono,0);
  const s=ctx.createBufferSource();
  s.buffer=b;
  s.connect(ctx.destination);
  s.start();
  pad.classList.add('active');
  setTimeout(()=>pad.classList.remove('active'),65);
}

function bindPads(){
  document.querySelectorAll('.pad').forEach(p=>{
    if(p.dataset.bound) return;
    p.dataset.bound='1';
    const fire=e=>{e.preventDefault();playVoiceNow(Number(p.dataset.voice),p)};
    p.addEventListener('touchstart',fire,{passive:false});
    p.addEventListener('pointerdown',e=>{if(e.pointerType!=='touch')fire(e)},{passive:false});
    p.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' ')fire(e)});
  });
}

function bindUnlock(){
  const b=document.getElementById('audioUnlock');
  if(!b) return;
  const fire=e=>{e.preventDefault();unlockAudio()};
  b.addEventListener('touchend',fire,{passive:false});
  b.addEventListener('click',fire);
}

document.addEventListener('DOMContentLoaded',bindUnlock);

Module.onRuntimeInitialized=()=>{
  renderHit=Module.cwrap('crispy_render_hit','number',['number','number','number','number']);
  bindPads();
};
