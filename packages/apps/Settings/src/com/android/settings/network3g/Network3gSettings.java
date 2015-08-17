package com.android.settings.network3g;

import com.android.settings.ProgressCategoryGeneric;
import com.android.settings.R;
import com.android.settings.SecuritySettings;

import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.SystemProperties;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.preference.CheckBoxPreference;
import android.provider.Settings;
import android.security.KeyStore;
import android.util.Log;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView;
import android.widget.Toast;
import android.widget.AdapterView.AdapterContextMenuInfo;

import java.util.Set;
import java.util.WeakHashMap;
import android.content.SharedPreferences;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.Context;
import java.util.Iterator;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import android.view.View.OnLongClickListener;
import com.android.settings.SettingsPreferenceFragment;

public class Network3gSettings extends SettingsPreferenceFragment implements Network3gLayer.Callback, DialogInterface.OnDismissListener{

    private static final String TAG = "Network3gSettingsFragment";
    private static final String KEY_ADD_OTHER_NETWORK 		= "network3g_add_other_network"; 
	private static final String KEY_PROG_NETWORK_3G_CONNECT = "network3gcon"; 
	private static final String RECEIVE_3G_STATE_CHANGE 	= "android.net.3g.STATE_CHANGE";
	private static final String RECEIVE_AIRPLANE_MODE_DONE  = "android.intent.action.PLANE_DONE";
	private Dialog mDialog;
	
    private static final int con_3gdown = 0;
    private static final int con_3gup = 1;

	private Preference mAddOtherNetwork;
	private ProgressCategoryGeneric mApCategory;
//	private ProgressCategory mConnectCategory;
     public Context mcontext;

    private WeakHashMap<Network3gConState, Network3gConPreference> mAps;
	private Network3gLayer mNetwork3gLayer;			// hoop

    public Network3gSettings() {
				
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {

             mcontext=getActivity();
		mAps = new WeakHashMap<Network3gConState, Network3gConPreference>();	
		mNetwork3gLayer = new Network3gLayer(mcontext, this);
		if (checkAirplaneModeOn(mcontext)) {
			DisplayToast(
				this.getResources().getString(R.string.network_on_airplane_mode));   
			finish();
		}

        super.onCreate(savedInstanceState);
        onCreatePreferences();								
		mNetwork3gLayer.restoreNetwork();	

		 
    }

	private void DisplayToast(String msg) {
		Toast.makeText(mcontext, msg, 
		     Toast.LENGTH_SHORT).show();        
    }

	private static boolean checkAirplaneModeOn(Context context) {
        return Settings.System.getInt(context.getContentResolver(),
                Settings.System.AIRPLANE_MODE_ON, 0) != 0;
    }
	
    private void onCreatePreferences() {
        addPreferencesFromResource(R.xml.network3g_settings);
		
		final PreferenceScreen preferenceScreen = getPreferenceScreen();
        mApCategory = 
			(ProgressCategoryGeneric) preferenceScreen.findPreference("network3gconnect");
		mApCategory.setOrderingAsAdded(false);
		
		mAddOtherNetwork = preferenceScreen.findPreference(KEY_ADD_OTHER_NETWORK);
    }

    static boolean isAirplaneModeOn(Context context){
        return Settings.System.getInt(context.getContentResolver(),
	                                      Settings.System.AIRPLANE_MODE_ON, 
	                                      0) != 0;
    }

    private BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver(){

        public void onReceive(Context context, Intent intent){
        	String action = intent.getAction();

			// receive from server.
			if (action.equals(RECEIVE_3G_STATE_CHANGE)) {
	            final int net_3g_status = (int)intent.getIntExtra("3g_status",0);
				Log.d(TAG,"=========11 RECEIVE_3G_STATE_CHANGE net_3g_status "+net_3g_status+ " ========");
				if(net_3g_status == 0)
					mHandler.sendEmptyMessage(con_3gdown);
				else{
					Log.d(TAG,"========= RECEIVE_3G_STATE_CHANGE net_3g_status "+net_3g_status+ " ========");
					mHandler.sendEmptyMessage(con_3gup);
				}
			}

			// airplane mode on or off
			if (action.equals(RECEIVE_AIRPLANE_MODE_DONE)) {
				if (isAirplaneModeOn(context)) {
					findPreference(KEY_ADD_OTHER_NETWORK).setEnabled(false);
					findPreference(KEY_PROG_NETWORK_3G_CONNECT).setEnabled(false);		
				} else {
					findPreference(KEY_ADD_OTHER_NETWORK).setEnabled(true);
					findPreference(KEY_PROG_NETWORK_3G_CONNECT).setEnabled(true);						
				}
			}
				
        }
    };
	
    @Override
    public void onResume() {
        super.onResume();	
		IntentFilter filter = new IntentFilter();
		filter.addAction(RECEIVE_3G_STATE_CHANGE);
		filter.addAction(RECEIVE_AIRPLANE_MODE_DONE);
		mcontext.registerReceiver(mBroadcastReceiver,new IntentFilter(filter));
    }

    @Override
    public void onPause() {
        super.onPause();
		mcontext.unregisterReceiver(mBroadcastReceiver);		
    }
	
    private Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
                network3gstat_ctrl(msg.what);
        }
    };

	private void network3gstat_ctrl(int enable){

		Network3gConPreference pref = null;
		Set set = mAps.keySet();
		
		if(set == null)
			return;
		
		Iterator iterator = set.iterator();
		if(iterator==null){
			return;
		}
		
		while (iterator.hasNext()) {
			pref = mAps.get(iterator.next());

			if (pref == null)
				break;
			
			if (enable == 1) {
				if(pref.getNetwork3gConState().configured == 
					Network3gConState.config_connecting)
					break;
			}else{
				if(pref.getNetwork3gConState().configured == 
					Network3gConState.config_connected ||
					pref.getNetwork3gConState().configured == 
					Network3gConState.config_connecting)
					break;
			}
		}

		if(pref != null &&
				enable == 0 &&(
				pref.getNetwork3gConState().configured == 
					Network3gConState.config_connected 		||
				pref.getNetwork3gConState().configured == 
					Network3gConState.config_connecting)){
			mAps.remove(pref);
			pref.getNetwork3gConState().configured = 
								Network3gConState.config_disconnect;
			mAps.put(pref.getNetwork3gConState(), pref);
			pref.refreshNetwork3gConState();
		}

		Log.d(TAG,"========== enale="+enable+" pref.getNetwork3gConState().configured = "+pref.getNetwork3gConState().configured+" ==========");
		
		if(pref != null &&
				enable == 1 &&
				(pref.getNetwork3gConState().configured == 
				Network3gConState.config_connecting)){
			Log.d(TAG,"========!! enale="+enable+" pref.getNetwork3gConState().configured = "+pref.getNetwork3gConState().configured+" ==========");
			mAps.remove(pref);
			pref.getNetwork3gConState().configured = 
						Network3gConState.config_connected;
			mAps.put(pref.getNetwork3gConState(), pref);
			pref.refreshNetwork3gConState();
		}
		
	}

    //============================
    // Preference callbacks
    //============================
    
    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        super.onPreferenceTreeClick(preferenceScreen, preference);

        if (preference == mAddOtherNetwork) {
            showAddOtherNetworkDialog();
        } else if (preference instanceof Network3gConPreference) {
            Network3gConState state = 
				((Network3gConPreference) preference).getNetwork3gConState();
            showNetwork3gConDialog(state, Network3gConDialog.MODE_INFO);
        }        
        return false;
    }
	
    public void showNetwork3gConDialog(Network3gConState state, int mode) {
        Network3gConDialog dialog = new Network3gConDialog(mcontext, mNetwork3gLayer);
        dialog.setMode(mode);
        dialog.setState(state);
		dialog.setTitle(R.string.network3g);
        showDialog(dialog);
    }

    private void showAddOtherNetworkDialog() {
        Network3gConDialog dialog = new Network3gConDialog(mcontext,mNetwork3gLayer);
        dialog.setState(new Network3gConState(mcontext));
        //dialog.setMode(AccessPointDialog.MODE_CONFIGURE);
        dialog.setTitle(R.string.network3g_add_other_network);
        showDialog(dialog);
    }
    
	private void showDialog(Dialog dialog) {
        // Have only one dialog open at a time
        if (mDialog != null) {
            mDialog.dismiss();
        }
        
        mDialog = dialog;
        dialog.setOnDismissListener(this);
        if (dialog != null) {
            dialog.show();
        }
    }

	public void onDismiss(DialogInterface dialog){
	}

    public void onNetwork3gConSetChanged(Network3gConState ap, boolean added) {
        Network3gConPreference pref = mAps.get(ap);
        
        Log.v(TAG, "onNetwork3gConSetChanged with " + ap + " and "
                + (added ? "added" : "removed") + ", found pref " + pref);
		
        if (added) {
            if (pref == null) {
                pref = new Network3gConPreference(this, ap);
                mAps.put(ap, pref);
            } else {
                pref.setEnabled(true);
            }
            mApCategory.addPreference(pref);


			
        } else {
            mAps.remove(ap);            
            if (pref != null) {
                mApCategory.removePreference(pref);
            }            
        }
    }

    public void onNetwork3gConStateChanged(boolean enabled) {
        if (enabled) {
            mApCategory.setEnabled(true);
        } else {
            mApCategory.removeAll();
            mAps.clear();
        }

        mAddOtherNetwork.setEnabled(enabled);
    }
	
    public void onRetryPassword(Network3gConState ap) {
        if ((mDialog != null) && mDialog.isShowing()) {
            // If we're already showing a dialog, ignore this request
            return;
        }        
        //showNetwork3gConDialog(ap, Network3gConDialog.MODE_RETRY_PASSWORD);
    }	

    public void onScanningStatusChanged(boolean started) {
        mApCategory.setProgress(started);
    }    

    public void onError(int messageResId) {
        //Toast.makeText(this, messageResId, Toast.LENGTH_LONG).show();
    }
	
}

