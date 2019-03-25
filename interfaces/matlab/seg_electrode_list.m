function [elecs, nodes] = seg_electrode_list(electrodes, frac)
    num_elec = 0; %after segmentation
	for i=1:length(electrodes)
		%TODO store in array to avoid repeated calculations
		num_elec = num_elec + ceil(electrodes(i).length/frac);
    end
    elecs = repmat(new_electrode(), num_elec); %pre-allocate
	e = 1;
	for i=1:length(electrodes)
		ns = ceil(electrodes(i).length/frac);
		[new_elecs, new_nodes] = segment_electrode(electrodes(i), ns);
		for k=1:ns
			elecs(e) = new_elecs(k);
			e = e + 1;
        end
        if i == 1
           nodes = new_nodes;
        else
            dim = size(new_nodes);
            for k=1:dim(1)
                if ~ismember(new_nodes(k,:), nodes, 'rows')
                    nodes = cat(1, nodes, new_nodes(k,:));
                end
            end
        end
	end
end