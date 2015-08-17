package com.android.settings.network3g;

import com.android.settings.R;
import android.content.Context;
import android.net.NetworkInfo;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Log;
import java.io.*;  
import android.os.*;


public final class Network3gConState implements Comparable<Network3gConState>, Parcelable {

	// don't hard code here.
	/*
    public static final String DeviceHW_E220 = "DeviceHW_E220";
    public static final String DEviceVIA = "DeviceVIA";
    public static final String DEviceDatang = "DeviceDATANG";
    */
    
	public static final int config_disconnect=0;
	public static final int config_connecting=1;
	public static final int config_connected=2;
	public String index;
    public String name;
    public String number;
    public String user;
    public String passwd;
    public String device;
    public String apn;
	public String pin_code;
	
    public boolean primary;
    public boolean seen;
    public int configured = config_disconnect;
    
    private Network3gConStateCallback mCallback;
    private Context mContext;
		
    public Network3gConState(Context context) {
        this();        
        setContext(context);
    }
	
    public Network3gConState() {

    }	
	
    void setContext(Context context) {
        mContext = context;
    }

	public void setIndex(String index) {
        if (index != null) {
            this.index= (index);
            requestRefresh();
        }
    }

	public void setPinCode(String pincode) {
        //if (pincode != null) {
            this.pin_code = (pincode);
            requestRefresh();
        //}
    }
	
    public void setName(String name) {
        if (name != null) {
            this.name = (name);
            requestRefresh();
        }
    }

    public void setNumber(String number) {
        if (number != null) {
            this.number = (number);
            requestRefresh();
        }
    }

    public void setUser(String user) {
        if (user != null) {
            this.user = (user);
            requestRefresh();
        }
    }

    public void setPasswd(String passwd) {
        if (passwd != null) {
            this.passwd = (passwd);
            requestRefresh();
        }
    }

    public void setAPN(String apn) {
        if (apn != null) {
            this.apn = (apn);
            requestRefresh();
        }
    }
	
    public void setDevice(String device) {
        if (device != null) {
            this.device = (device);
            requestRefresh();
        }
    }


    public void setCallback(Network3gConStateCallback callback) {
        mCallback = callback;
    }
	
    interface Network3gConStateCallback {
		// update the UI
        void refreshNetwork3gConState();
    }	

    private void requestRefresh() {        
        if (mCallback != null) {
            mCallback.refreshNetwork3gConState();
        }
    }
    
    public static String convertToQuotedString(String string) {
        if (TextUtils.isEmpty(string)) {
            return "";
        }
        
        final int lastPos = string.length() - 1;
        if (lastPos < 0 || (string.charAt(0) == '"' && string.charAt(lastPos) == '"')) {
            return string;
        }
        
        return "\"" + string + "\"";
    }	

    public int compareTo(Network3gConState other) {
        // Alphabetical
        return name.compareToIgnoreCase(other.name);
    }
	
    /** Implement the Parcelable interface */
    public void writeToParcel(Parcel dest, int flags) {
    	dest.writeString(index);
        dest.writeString(name);
        dest.writeString(number);
        dest.writeString(user);
        dest.writeString(passwd);
        dest.writeString(device);
        dest.writeString(apn);
		dest.writeString(pin_code);
	}


    public int describeContents() {
        return 0;
    }

    public boolean isConnectable() {
        return (configured == Network3gConState.config_disconnect);
    }

	private boolean isRusAPN(){
		return ((name.equals("Beeline")&&apn.equals("internet.beeline.ru")) ||
					(name.equals("megafon")&&apn.equals("internet")) ||
					(name.equals("MTS")&&apn.equals("internet.mts.ru")));
	}

    /**
     * @return Whether this AP can be forgotten at the moment.
     */
    public boolean isForgetable() {
    	if (!index.equals(null) && 
			(Integer.parseInt(index)<3) && 
			isRusAPN()){
			return false;
		}
        return true;
    }

    public void setConfigured(int configured) {
        if (this.configured != configured) {
            this.configured = configured;
            requestRefresh();
        }
    }	
}


