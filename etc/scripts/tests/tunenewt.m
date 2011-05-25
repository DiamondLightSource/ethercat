function [tune, peak] = tunenewt(z)
N = length(z);
S = 10;
dt = 1/N;
hann_window = 1 - cos(2*pi*(0:N-1)/N);
z = z .* hann_window;
zf = fft(z);
[fmax, k] = max(abs(zf));
tuneguess = (k-1)*dt;
if tuneguess > 0.5
    tuneguess = tuneguess - 1;
end
tunes = zeros(1, S);
peaks = zeros(1, S);
df = @(v) dft(v, z);
tuneb = linspace(tuneguess - dt, tuneguess + dt, S+1);
for s = 1:S
    tune1 = tuneb(s);
    tune2 = tuneb(s+1);
    try
        tunes(s) = fzero(df, [tune1, tune2]);
        [zeroval, peaks(s)] = df(tunes(s));
    catch ex
        if ~strcmp(ex.identifier, 'MATLAB:fzero:ValuesAtEndPtsSameSign')
            rethrow(ex);
        end
    end
end
[peak, ti] = max(peaks);
tune = tunes(ti);
end

function [df, mag] = dft(tune, z)
N = length(z);
a = -2j*pi*tune*(0:N-1);
g = sum(z.*exp(a));
dg = sum(a.*z.*exp(a));
mag = abs(g);
% f has the same roots as mag but is smoother
% f = conj(g)*g;
df = conj(g)*dg + conj(dg)*g;
% ddf = 2 * dg*conj(dg) + ddg*conj(g) + g*conj(ddg);
end
