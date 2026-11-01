import numpy as np
sr=48000
def lpc_peaks(x, order=40):
    x=x[24000:24000+8192]*np.hanning(8192)
    # downsample-free LPC via autocorrelation
    r=np.correlate(x,x,'full')[len(x)-1:len(x)+order]
    R=np.array([[r[abs(i-j)] for j in range(order)] for i in range(order)])
    a=np.linalg.solve(R+1e-9*np.eye(order), r[1:order+1])
    w=np.linspace(0,4000,4001)
    A=1-sum(a[k]*np.exp(-2j*np.pi*w*(k+1)/sr) for k in range(order))
    H=20*np.log10(1/np.abs(A))
    pk=[w[i] for i in range(1,len(w)-1) if H[i]>H[i-1] and H[i]>H[i+1] and H[i]>H.max()-25]
    return pk
def f0(x):
    x=x[24000:24000+4800]; ac=np.correlate(x,x,'full')[len(x)-1:]
    lo,hi=int(sr/400),int(sr/60); l=lo+np.argmax(ac[lo:hi]); return sr/l
v=np.fromfile('vowel.f32',np.float32)
print('orig  f0=%.1f Hz formants'%f0(v), [round(p) for p in lpc_peaks(v)])
for s in ['-4','+4','+7']:
    y=np.fromfile(f'out_{s}.f32',np.float32)
    r=2**(int(s)/12)
    print(f'{s:>3} st f0=%.1f Hz formants'%f0(y), [round(p) for p in lpc_peaks(y)], ' expected', [round(f*r) for f in (700,1200,2600)])
