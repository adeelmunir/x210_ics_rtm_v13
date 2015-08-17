package com.android.settings.network3g;

import com.android.settings.R;
import com.android.settings.*;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.NetworkInfo;
import android.net.NetworkInfo.DetailedState;
import android.net.NetworkInfo.State;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.Message;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Config;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.io.*;  
import android.os.*;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.content.*;
import android.database.Cursor;
import android.content.SharedPreferences;
import android.widget.Toast;
import android.widget.TextView;
import android.widget.LinearLayout;
import android.view.Gravity;


/**
 * Helper class for abstracting Network 3G.
 * <p>
 * Client must call {@link #onCreate()}, {@link #onCreatedCallback()},
 * {@link #onPause()}, {@link #onResume()}.
 */
public class Network3gLayer {
    private static final String TAG = "Network3gLayer";

    static final boolean LOGV = true || Config.LOGV;
	static final int max_entry = 1000;
	private Callback mCallback;
	private Context mContext;
	
    interface Callback {
        void onError(int messageResId);
        
        /**
         * Called when an AP is added or removed.
         * 
         * @param ap The AP.
         * @param added {@code true} if added, {@code false} if removed.
         */
        void onNetwork3gConSetChanged(Network3gConState ap, boolean added);
        
        /**
         * Called when the scanning status changes.
         * 
         * @param started {@code true} if the scanning just started,
         *            {@code false} if it just ended.
         */
        void onScanningStatusChanged(boolean started);

        /**
         * Called when the access points should be enabled or disabled. This is
         * called from both wpa_supplicant being connected/disconnected and Wi-Fi
         * being enabled/disabled.
         * 
         * @param enabled {@code true} if they should be enabled, {@code false}
         *            if they should be disabled.
         */
        void onNetwork3gConStateChanged(boolean enabled);   
        
        /**
         * Called when there is trouble authenticating and the retry-password
         * dialog should be shown.
         * 
         * @param ap The access point.
         */
        void onRetryPassword(Network3gConState ap);
    }
 

    public Network3gLayer(Context context, Callback callback) {
		Log.e(TAG, "Network3gLayer construct enter"+context);
        mContext = context;
        mCallback = callback;
		//restoreNetwork();
    }

	public void Network3gConSetChanged(Network3gConState ap, boolean added){
        if (mCallback != null) {
            mCallback.onNetwork3gConSetChanged(ap, true);
        }
	}

	/*
	* Function: load the connect state from xlm file. 
	*/
	public boolean restoreNetwork() {
		SharedPreferences store_config_num;
		SharedPreferences store_config;
		String numS, index;
		int numI = 0;
		int i;		
		String name;
		boolean on;

		// first check if pppd is on  ... 
		// on = SystemProperties.get("net.pppd.success", " ").equals("ok");
		
		Network3gConState state;
		store_config_num = mContext.getSharedPreferences("num", 0);
		numS = store_config_num.getString("num", "");
		
		if ( numS.length() > 0 ) {
			numI = Integer.parseInt(numS);
			
			for (i=0; i<numI; i++) {
				store_config = mContext.getSharedPreferences(Integer.toString(i), 0);
				index = store_config.getString("index", "");
				if (index.length() > 0){					
					state 			= new Network3gConState();
					state.index		= index;
					state.name 		= store_config.getString("name", "");
					state.device	= store_config.getString("device", "");
					state.number 	= store_config.getString("number", "");
					state.user 		= store_config.getString("user", "");
					state.passwd 	= store_config.getString("passwd", "");
					state.apn 		= store_config.getString("apn", "");
					state.pin_code	= store_config.getString("pincode", "");

					if (SystemProperties.get("net.pppd.success", " ").equals("ok")) {
						if(store_config.getInt("configured", 
								Network3gConState.config_disconnect) == 
								Network3gConState.config_connecting){
							state.configured = Network3gConState.config_connected;
						}
					} else if (SystemProperties.get("net.pppd.success", " ").equals("connecting")) {
						// I think connecting is also acceptable.
						// TODO nothing
					} else {
						state.configured = Network3gConState.config_disconnect;
					}					
					Network3gConSetChanged(state, true);
				}				
			}
		}		
		return true;
	}

	public boolean forgetNetwork(Network3gConState state) {
		SharedPreferences store_config_num;
		String 	numS;
		int 	numI;
		
        if (mCallback != null) {
            mCallback.onNetwork3gConSetChanged(state, false);
        }

		store_config_num = mContext.getSharedPreferences("num", 0);
		SharedPreferences.Editor editor = store_config_num.edit();
		numS = store_config_num.getString("num", "");
		if (numS.length() <= 0) {
			Log.e(TAG, "Can't find index "+numS+" when forget network");
			return true;
		} else {
			numI = Integer.parseInt(numS);			
			numI--;
			editor.putString("num", Integer.toString(numI));
			editor.commit();
		}

		File base = mContext.getFilesDir();
		//mContext.deleteFile(base.getParent()+"/shared_prefs/"+state.index+".xml");
		File fuck = new File(base.getParent()+"/shared_prefs/"+state.index+".xml");
		fuck.delete();
		return true;
	}

	public boolean saveConState(Network3gConState state) {
		SharedPreferences store_config_num;
		SharedPreferences store_config;
		SharedPreferences.Editor editor;
		String 	num;		
		String 	indexS;
		int 	indexI = 0;
		int 	i;
		String 	name;

		store_config_num = mContext.getSharedPreferences("num", 0);
		num = store_config_num.getString("num", "");
		if ( num.length() <= 0 ) {
			Log.e(TAG, "There is no connection named" + state.name+ " saved");
			return true;
		} 

		// We assume that at that time this file must exist.		
		store_config = mContext.getSharedPreferences(state.index, 0);
		editor = store_config.edit();
		editor.putInt("configured", state.configured);
		
		editor.commit();
		return true;
	}

	public boolean saveNetwork(Network3gConState state) {
		SharedPreferences store_config_num;
		SharedPreferences store_config;
		SharedPreferences.Editor editor;
		
		String 	numS;
		int 	numI;
		int 	insert_index = 0;	
		
		store_config_num = mContext.getSharedPreferences("num", 0);
		numS = store_config_num.getString("num", "");
		if( numS.length() > 0 ){
			numI = Integer.parseInt(numS);				
			insert_index = numI;
			numI++;
		} else {
			insert_index = 0;
			numI = 1;
		}

		store_config = mContext.getSharedPreferences(Integer.toString(insert_index), 0);
		editor = store_config.edit();	
		state.index = Integer.toString(insert_index);
		editor.putString("index", 	Integer.toString(insert_index));
		editor.putString("name", 	state.name);
		editor.putString("device", 	state.device);
		editor.putString("number", 	state.number);
		editor.putString("user", 	state.user);
		editor.putString("passwd", 	state.passwd);
		editor.putString("apn", 	state.apn);		
		editor.putString("pincode", state.pin_code);
		editor.putInt("configured", state.configured);
		editor.commit();

		store_config_num = mContext.getSharedPreferences("num", 0);
		editor = store_config_num.edit();		
		editor.putString("num", Integer.toString(numI));
		editor.commit();
		
		return true;
	}

	private void DisplayToast(String msg) {
		Toast toast = Toast.makeText(mContext, msg, Toast.LENGTH_SHORT);
		//((TextView)((LinearLayout)toast.getView()).getChildAt(0)).setGravity(Gravity.CENTER_HORIZONTAL);
		toast.show();
    }    
	
    private boolean setNetwork3gOn(boolean enabling, Network3gConState state) {
        Log.v(TAG, "setNetwork3gOn enter "+enabling);

		if (enabling == true){			
			java.lang.Process proc_enable = null; 

			if (!(SystemProperties.get("net.pppd.active.index", "").equals(state.index)) &&
					!(SystemProperties.get("net.pppd.active.index", "").equals(""))) {
				
				DisplayToast(
					mContext.getResources().getString(R.string.network_not_available));   

				state.setConfigured(Network3gConState.config_disconnect);
				
				return true;
			}
			
			try { 
				Log.v(TAG, "settting pppd successs");
				SystemProperties.set("net.pppd.success",		"connecting");
				SystemProperties.set("net.pppd.device",			state.device);
				SystemProperties.set("net.pppd.user",			state.user);
				SystemProperties.set("net.pppd.passwd",			state.passwd);
				SystemProperties.set("net.pppd.number",			state.number);
				SystemProperties.set("net.pppd.apn",			state.apn);
				SystemProperties.set("net.pppd.active.index",	state.index);
				SystemProperties.set("net.pppd.pin.code",		state.pin_code);
				SystemProperties.set("ctl.start", "pppd");
				// Process.waitFor only put the command to shell, and return alone.
				state.setConfigured(Network3gConState.config_connecting);
			} catch (Exception e) {       
				e.printStackTrace();  
			} 
			return true;
			
		} else {

			if (SystemProperties.get("net.pppd.active.index", "").equals(state.index)) {
				SystemProperties.set("ctl.stop", "pppd");				
				SystemProperties.set("net.pppd.active.index","");
			}		
			
			state.setConfigured(Network3gConState.config_disconnect);
			return true;
		}

		//return true;
    }
	
   public boolean connectToNetwork(Network3gConState state, boolean connect) {
		File file = new File("/dev/ttyUSB0");
		File file_2 = new File("/dev/ttyACM0");
		boolean exists = file.exists() || file_2.exists();

		Log.i(TAG, "/dev/ttyUSB0 exist is "+file.exists()+" "+"/dev/ttyACM0 "+file_2.exists());
		
		if (exists && connect) {
			Log.i(TAG, "There is one PPP device attatch!");
			DisplayToast(
        			mContext.getResources( ).getString(R.string.pppdmode_connet)); 
		} else if (connect){			
		    if (SystemProperties.get("net.pppd.modem.status", " ").equals("0")) {
                DisplayToast(
                   mContext.getResources( ).getString(R.string.pppdmode_unconnet)); 
                   //return false;
		    } 
		    if (SystemProperties.get("net.pppd.modem.status", " ").equals("1")) {
               DisplayToast(
                     mContext.getResources( ).getString(R.string.pppdmode_apped)); 
               //return false; 
		    } 
		}
		
        if (LOGV) {
            Log.v(TAG, "connectToNetwork to ... ");
        }

		setNetwork3gOn(connect, state);
		saveConState(state);
		Log.v(TAG, "connectToNetwork to ... "+state.configured);
        return true;
    }
}

