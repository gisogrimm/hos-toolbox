function create_sounds( name, f0, d, vPitch, tag, bNoise )
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
  if nargin < 6
    bNoise = false;
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
    if bNoise
      noisesound(x0,fs,ofname,d);
    else
      pulsesound(x0,fs,ofname,d);
    end
    fprintf(fh,'%s\n',ofname);
  end
  fclose(fh);
