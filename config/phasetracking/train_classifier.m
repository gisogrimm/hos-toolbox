function sTable = train_classifier
  d = dir('pattern_??_feat.mat');
  %sFeat = 'l_level_db';
  %sFeat = 'l_level';
  %sFeat = 'sync';
  sFeat = 'ifc_chroma';
  nClasses = 12;
  vRClass = [];
  if( numel(d) > 0)
    csFeat = cell(nClasses,1);
    s = load(d(1).name);
    mFeature = s.(sFeat);
    mFeature = mFeature(:,[2,4,6,7,9,11]);
    nDim = size(mFeature,2);
    mFeatTot = zeros(0,nDim);
    for k=1:numel(d)
      s = load(d(k).name);
      mFeature = s.(sFeat);
      mFeature = mFeature(:,[2,4,6,7,9,11]);
      N2 = size(mFeature,1);
      N = floor(N2/2);
      mFeature(1:(N2-N),:) = [];
      mFeatTot = [mFeatTot;mFeature];
      vRClassL = zeros(N,1);
      for kCl=1:nClasses
	idx = [(1+floor((kCl-1)*N/nClasses)):floor(kCl*N/nClasses)];
	if k==1
	  csFeat{kCl} = mFeature(idx,:);
	else
	  csFeat{kCl}(end+idx,:) = mFeature(idx,:);
	end
	vRClassL(idx) = kCl;
      end
      vRClass = [vRClass;vRClassL];
    end
    sTable = ...
	classifier_create(csFeat, 100, 0.2, cellstr(num2str([1:nDim]')));
    vClass = classifier( mFeatTot, sTable, 'prod' );
    disp(sprintf('prod: %g correct',sum(vClass==vRClass)/numel(vRClass)*100));
    vClass = classifier( mFeatTot, sTable, 'mean' );
    disp(sprintf('mean: %g correct',sum(vClass==vRClass)/numel(vRClass)*100));
  else
    error('no training data');
  end
  