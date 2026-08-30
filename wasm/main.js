let ctx=null;
let renderHit=null;
let outPtr=0;
let outCapacity=0;

function ensureAudio(){
  if(!ctx) ctx=new (window.AudioContext||window.webkitAudioContext)();
  if(ctx.state==='suspended') ctx.resume();
}

function ensureBuffer(frames){
  if(frames<=outCapacity) return;
  if(outPtr) Module._free(outPtr);
  outCapacity=frames;
  outPtr=Module._malloc(frames*4);
}

function playVoice(index,pad){
  ensureAudio();
  const sr=ctx.sampleRate;
  const frames=Math.ceil(sr*0.8);
  ensureBuffer(frames);
  const written=renderHit(index,outPtr,frames,sr);
  const mono=new Float32Array(Module.HEAPF32.buffer,outPtr,written).slice();
  const b=ctx.createBuffer(1,written,sr);
  b.copyToChannel(mono,0);
  const s=ctx.createBufferSource();
  s.buffer=b;
  s.connect(ctx.destination);
  s.start();
  pad.classList.add('active');
  setTimeout(()=>pad.classList.remove('active'),65);
}

Module.onRuntimeInitialized=()=>{
  renderHit=Module.cwrap('crispy_render_hit','number',['number','number','number','number']);
  document.querySelectorAll('.pad').forEach(p=>{
    const fire=e=>{e.preventDefault();playVoice(Number(p.dataset.voice),p)};
    p.addEventListener('pointerdown',fire,{passive:false});
    p.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){fire(e)}});
  });
};
