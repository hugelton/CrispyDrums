let ctx=null;
let renderHit=null;
let outPtr=0;
let outCapacity=0;
let unlocked=false;

window.Module=window.Module||{};

function ensureAudioSync(){
  if(!ctx) ctx=new (window.AudioContext||window.webkitAudioContext)();
  if(!unlocked){
    // iOS Safari requires an audible graph to be started directly inside
    // the user gesture. Prime the context synchronously with a one-frame buffer.
    const b=ctx.createBuffer(1,1,ctx.sampleRate);
    const s=ctx.createBufferSource();
    s.buffer=b;
    s.connect(ctx.destination);
    s.start(0);
    unlocked=true;
  }
  if(ctx.state==='suspended') ctx.resume();
  return ctx;
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
  const audio=ensureAudioSync();
  if(!renderHit) return;
  const sr=audio.sampleRate;
  const frames=Math.ceil(sr*0.8);
  ensureBuffer(frames);
  const written=renderHit(index,outPtr,frames,sr);
  const heap=heapF32();
  if(!heap||written<=0) return;
  const start=outPtr>>2;
  const mono=new Float32Array(written);
  mono.set(heap.subarray(start,start+written));
  const b=audio.createBuffer(1,written,sr);
  b.copyToChannel(mono,0);
  const s=audio.createBufferSource();
  s.buffer=b;
  s.connect(audio.destination);
  s.start(0);
  pad.classList.add('active');
  setTimeout(()=>pad.classList.remove('active'),65);
}

function bindPads(){
  document.querySelectorAll('.pad').forEach(p=>{
    if(p.dataset.bound) return;
    p.dataset.bound='1';
    const fire=e=>{
      e.preventDefault();
      playVoiceNow(Number(p.dataset.voice),p);
    };
    p.addEventListener('touchstart',fire,{passive:false});
    p.addEventListener('pointerdown',e=>{
      if(e.pointerType==='touch') return;
      fire(e);
    },{passive:false});
    p.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' ')fire(e)});
  });
}

Module.onRuntimeInitialized=()=>{
  renderHit=Module.cwrap('crispy_render_hit','number',['number','number','number','number']);
  bindPads();
};
