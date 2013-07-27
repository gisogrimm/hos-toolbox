function create_sounds( name, f0, d, vPitch, tag )
  if nargin < 1
    name = 'picforth5';
  end
  if nargin < 2
    f0 = 0;
  end
  if nargin < 3
    d = 4;
  end
  if nargin < 4
    vPitch = [-9,-7,-5,-4,-2,0,1,3,5]+f0;
  end
  if nargin < 5
    tag = '';
  end
  [x,fs] = wavread([name,'.wav']);
  fh = fopen(sprintf('%s_%s.soundfont',name,tag),'w');
  for k=1:numel(vPitch)
    if vPitch(k) ~= 0
      x0 = resample(x,round(2^(-vPitch(k)/12)*512),512);
    else
      x0 = x;
    end
    ofname = sprintf('%s_%s_%d.wav',name,tag,k);
    pulsesound(x0,fs,ofname,d);
    fprintf(fh,'%s\n',ofname);
  end
  fclose(fh);

function pulsesound( x,fs, sOutput, vdur )
  y = [];
  for k=1:numel(vdur)
    dur=vdur(k);
    %[x,fs] = wavread(sInput);
    dur = round(dur*fs);
    x(:,2:end) = [];
    x(dur+1:end) = [];
    x(end+1:dur) = 0;
    X = abs(realfft(x));
    X = X .* exp(-i*imag(hilbert(log(X))));
    x = realifft(X);
    x = 0.9*x / max(abs(x(:)));
    y = [y;x];
  end
  ramplen = 1400;
  wnd = hann(2*ramplen);
  y(end-(ramplen-1):end) = y(end-(ramplen-1):end) .* wnd(end-(ramplen-1):end);
  wavwrite(y,fs,sOutput);
  
function y = realfft( x )
% REALFFT - FFT transform of pure real data
%
% Usage: y = realfft( x )
%
% Returns positive frequencies of fft(x), assuming that x is pure
% real data. Each column of x is transformed separately.
  ;
  fftlen = size(x,1);
  
  y = fft(x);
  y = y([1:floor(fftlen/2)+1],:);

function x = realifft( y )
% REALIFFT - inverse FFT of positive frequencies in y
%
% Usage: x = realifft( y )
%
% Returns inverse FFT or half-complex spectrum. Each column of y is
% taken as a spectrum.
  ;
  channels = size(y,2);
  nbins = size(y,1);
  x = zeros(2*(nbins-1),channels);
  for ch=1:channels
    ytmp = y(:,ch);
    ytmp(1) = real(ytmp(1));
    ytmp(nbins) = real(ytmp(nbins));
    ytmp2 = [ytmp; conj(ytmp(nbins-1:-1:2))];
    x(:,ch) = real(ifft(ytmp2));
  end
