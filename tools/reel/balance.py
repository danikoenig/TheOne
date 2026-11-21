# Abschnitte des Reels auf gleiche Lautheit bringen (weiche Uebergaenge), dann WAV schreiben
import wave, numpy as np, sys
w=wave.open('out/audio.wav'); n=w.getnframes(); sr=w.getframerate()
raw=np.frombuffer(w.readframes(n),dtype=np.uint8).reshape(-1,3)
x=(raw[:,0].astype(np.int32)|(raw[:,1].astype(np.int32)<<8)|(raw[:,2].astype(np.int32)<<16))
x=np.where(x>=1<<23,x-(1<<24),x)/float(1<<23); x=x.reshape(-1,2).astype(np.float64)
bounds=[0,6,20,34,44,60,74,84,94,104,112,120,128,136,152,164,180]
target=-17.0
g=np.ones(len(x))
pts=[]
for a,b in zip(bounds[:-1],bounds[1:]):
    seg=x[int(a*sr):int(b*sr)]
    r=20*np.log10(np.sqrt((seg**2).mean())+1e-9)
    tg = target - 1.0 if a in (0, 60, 164) else target   # Pads 1 dB leiser
    gain=np.clip(tg-r, -6, 10)
    pts.append(((a+b)/2, gain)); print(a,b,round(r,1),'->',round(gain,1))
t=np.arange(len(x))/sr
gx=[p[0] for p in pts]; gy=[p[1] for p in pts]
# stufenweise je Abschnitt mit 0.4 s Uebergang
gdb=np.zeros(len(x))
for (a,b),(c,gn) in zip(zip(bounds[:-1],bounds[1:]),pts): gdb[int(a*sr):int(b*sr)]=gn
k=int(0.4*sr); ker=np.ones(k)/k
gdb=np.convolve(gdb,ker,mode='same')
y=x*(10**(gdb/20))[:,None]
y.astype('<f4').tofile('out/audio_bal.f32')
print('peak',np.abs(y).max())
