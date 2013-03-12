function sTable = test_classifier
  s = load('pattern_feat.mat');
  mFeature = s.l_level;
  %mFeature = s.ifc_chroma;
  %for k=1:size(mFeature,1)
  %  mFeature(k,:) = mFeature(k,:) / sum(mFeature(k,:));
  %end
  N = 251;
  nClasses = 12;
  csFeat = cell(nClasses,1);
  vRClass = zeros(N,1);
  for k=1:nClasses
    idx = [(1+floor((k-1)*N/nClasses)):floor(k*N/nClasses)];
    csFeat{k} = mFeature(idx,:);
    vRClass(idx) = k;
  end
  sTable = classifier_create(csFeat, 100, 0, cellstr(num2str([1:size(mFeature,2)]')));

%  s = load('patterntest_feat.mat');
%  mFeature = s.l_level;
%  for k=1:size(mFeature,1)
%    mFeature(k,:) = mFeature(k,:) / sum(mFeature(k,:));
%  end

  [vClass,vP] = classifier( mFeature, sTable, 'prod' );
  figure('name','prod');
  subplot(2,1,1);
  imagesc(20*log10(vP)');
  subplot(2,1,2);
  plot([vClass,vRClass]);
  disp(sprintf('%g correct',sum(vClass==vRClass)/N*100));
