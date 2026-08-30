let ctx=null;
let renderHit=null;
let outPtr=0;
let outCapacity=0;

window.Module=window.Module||{};

async function ensureAudio(){
  if(!ctx) ctx=new (window.AudioContext||window.webkitAudioContext)();
  if(ctx.state==='suspended') await ctx.resume();
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

async function playVoice(index,pad){
  await ensureAudio();
  if(!renderHit) return;
  const sr=ctx.sampleRate;
  const frames=Math.ceil(sr*0.8);
  ensureBuffer(frames);
  const written=renderHit(index,outPtr,frames,sr);
  const heap=heapF32();
  if(!heap || written<=0) return;
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
    const fire=e=>{e.preventDefault();playVoice(Number(p.dataset.voice),p).catch(console.error)};
    p.addEventListener('pointerdown',fire,{passive:false});
    p.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' ')fire(e)});
  });
}

Module.onRuntimeInitialized=()=>{
  renderHit=Module.cwrap('crispy_render_hit','number',['number','number','number','number']);
  bindPads();
};
