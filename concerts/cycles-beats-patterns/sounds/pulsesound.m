function pulsesound( x,fs, sOutput, vdur, bRMS )
  if nargin < 5
    bRMS = false;
  end
  y = [];
  rms = sqrt(mean(x(:,1).^2));
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
    if bRMS
      x = x * rms/sqrt(mean(x(:).^2));
    else
      x = 0.9*x / max(abs(x(:)));
    end
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
