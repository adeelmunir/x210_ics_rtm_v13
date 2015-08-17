package com.android.settings.network3g;

import com.android.settings.R;
import android.preference.Preference;
import android.view.View;
import android.widget.ImageView;
import android.content.Context;
import android.util.Log;

public class Network3gConPreference extends Preference implements
        Network3gConState.Network3gConStateCallback {
    
    // UI states
    
    //private static final int[] STATE_ENCRYPTED = { R.attr.state_encrypted };
    private static final int[] STATE_EMPTY = { };
    
    // Signal strength indicator
    private static final int UI_SIGNAL_LEVELS = 4;
	private static final String TAG = "Network3gSettings";
    
    private Network3gConState mState;

    public Network3gConPreference(Network3gSettings Settings, Network3gConState state) {
        super(Settings.mcontext, null);
        mState = state;
        
        state.setCallback(this);
        refresh();
    }
    
    public void refresh() {
		final Context context = getContext();
        setTitle(mState.name);
        Log.d(TAG,"====== mState.configured= "+mState.configured+" =======");
		switch(mState.configured){
			case Network3gConState.config_connected:
				setSummary(context.getString(R.string.network3g_connect));
				break;
			case Network3gConState.config_connecting:
				setSummary(context.getString(R.string.network3g_connecting));
				break;
			case Network3gConState.config_disconnect:
				setSummary(context.getString(R.string.network3g_disconnect));
				break;	
		}
        notifyChanged();
    }
    
    public void refreshNetwork3gConState() {
        refresh();
        
        // The ordering of access points could have changed 
        // due to the state change, so
        // re-evaluate ordering
        notifyHierarchyChanged();
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);
	}


    /**
     * Returns the {@link Network3gConState} associated with this preference.
     * @return The {@link Network3gConState}.
     */
    public Network3gConState getNetwork3gConState() {
        return mState;
    }

    @Override
    public int compareTo(Preference another) {
        if (!(another instanceof Network3gConPreference)) {
            // Let normal preferences go before us.
            // NOTE: we should only be compared to Preference in our
            // category.
            return 1;
        }
        
        return mState.compareTo(((Network3gConPreference) another).mState);
    }
}

