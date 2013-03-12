function sTable = classifier_create( cmParam, nBins, w_hist, labels )
% CLASSIFIER_CREATE - create a Bayes table for classification
%
% Usage:
% sTable = classifier_create( cmParam, nBins, w_hist, [labels] );
% 
% cmParam: cell array of parameter matrix, one entry for each class
% nBins  : number of bins per parameter dimension
% w_hist : weight of histogram (1) versus norm probability density
%          function (0)
% sTable : classification table
  
  nClasses = numel(cmParam);
  nParams = size(cmParam{1},2);
  if nargin < 4
    labels = {};
    for k=1:nParams
      labels{k} = sprintf('feature %d',k);
    end
  end
  mMin = zeros(nClasses,nParams);
  mMax = zeros(nClasses,nParams);
  for pdim = 1:nParams
    for vclass = 1:nClasses
      mMin(vclass,pdim) = min(cmParam{vclass}(:,pdim));
      mMax(vclass,pdim) = max(cmParam{vclass}(:,pdim));
    end
  end
  sTable.min = min(mMin);
  sTable.max = max(mMax)+eps;
  sTable.edges = zeros(nBins,nParams);
  for k=1:nParams
    sTable.edges(:,k) = linspace(sTable.min(k),sTable.max(k),nBins);
  end
  sTable.hist = zeros(nBins,nParams,nClasses);
  for kp=1:nParams
    for kc=1:nClasses
      vhist = histc(cmParam{kc}(:,kp),sTable.edges(:,kp));
      vhist = vhist / sum(vhist);
      vpdf = diff(sTable.edges(1:2,kp)) * ...
	     normpdf(sTable.edges(:,kp),...
		     mean(cmParam{kc}(:,kp)),...
		     std(cmParam{kc}(:,kp)));
      sTable.hist(:,kp,kc) = w_hist*vhist + (1-w_hist)*vpdf;
    end
  end
  sTable.tab = sTable.hist;
  for kp=1:nParams
    vtab_sum = zeros(nBins,1);
    for kc=1:nClasses
      vtab_sum = vtab_sum + sTable.hist(:,kp,kc);
    end
    sTable.tab(:,kp,:) = sTable.tab(:,kp,:) ./ ...
	repmat(vtab_sum,[1,1,nClasses]);
  end
  sTable.labels = labels;
  