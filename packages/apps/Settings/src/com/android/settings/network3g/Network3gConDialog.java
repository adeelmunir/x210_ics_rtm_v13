package com.android.settings.network3g;

import com.android.settings.R;
import com.android.settings.SecuritySettings;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.content.res.Configuration;
//import android.security.CertTool;
import android.security.Credentials;
import android.security.KeyStore;

//import android.security.Keystore;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.text.method.PasswordTransformationMethod;
import android.text.method.TransformationMethod;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.View.OnLongClickListener;

import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TableLayout;
import android.widget.TextView;
//import android.widget.AdapterView;
import android.content.SharedPreferences;
import android.os.*;
import java.util.ArrayList;
import android.util.Xml;
import com.android.internal.util.XmlUtils;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;


public class Network3gConDialog extends AlertDialog implements DialogInterface.OnClickListener,
        							AdapterView.OnItemSelectedListener, View.OnClickListener {

    private static final String TAG = "Network3gConDialog";
    private static final int POSITIVE_BUTTON = BUTTON1;
    private static final int NEGATIVE_BUTTON = BUTTON2;
    private static final int NEUTRAL_BUTTON = BUTTON3;
	private static final String PARTNER_APNS_PATH = "etc/modems-conf.xml";

	private	Context mContext;
	private View mView;

	private Boolean  mJust_load = false;

	private String 	 mIndex;
    private EditText mNameEdit;
    private EditText mNumberEdit;
    private EditText mUserEdit;
    private EditText mPasswdEdit;
    private EditText mAPNEdit;
	private EditText mPinCode;            	
	private CheckBox mdefault_user_pw;
	private ArrayAdapter<String> adapter;
	
    private Spinner mDeviceSpinner;
	private Network3gConState mState;
	private Network3gLayer mNetwork3gLayer;

    /** The dialog should show info connectivity functionality */
    public static final int MODE_INFO = 0;
    /** The dialog should configure the detailed con properties */
    public static final int MODE_CONFIGURE = 1;
	private int mMode = MODE_CONFIGURE;

    private int mConnectButtonPos = Integer.MAX_VALUE; 
    private int mForgetButtonPos = Integer.MAX_VALUE;
    private int mSaveButtonPos = Integer.MAX_VALUE;
	
    // Info-specific views
    private ViewGroup mTable;
	
	private ArrayList<ModemInfo> mModemList;
	private String mModemName[];

	private static class ModemInfo{
		public String connectName;     // what
		public String modemName;
		public String apn;
		public String dialNum;
		public String user;
		public String passwd;		
		public String index;
	}

	private int getModemOrderFromIndex(int index){
		ModemInfo info;
		for (int i=0; i<mModemList.size(); i++) {			
			info = mModemList.get(i);			
			if (Integer.parseInt(info.index) == index) {
				return i;
			}
		}
		return 0;
	}
	
    public Network3gConDialog(Context context, Network3gLayer Network3gLayer) {
        super(context);
		mNetwork3gLayer = Network3gLayer;
    }
	
    @Override
    protected void onCreate(Bundle savedInstanceState) {
	    mContext = getContext();
        onLayout();
        onFill();
        super.onCreate(savedInstanceState);
    }
	
    public void onClick(DialogInterface dialog, int which) {
			
        if (which == mForgetButtonPos) {
            handleForget();
        } else if (which == mConnectButtonPos) {
            if (mState.isConnectable()) {
		SystemProperties.set("au.ui.socket.pppd0","on");
            	handleSave();
            } else {
                          SystemProperties.set("au.ui.socket.pppd0","off");
				handleConnect();
			}
        } else if (which == mSaveButtonPos) {
            handleSave();
        }		
    }

    public void onNothingSelected(AdapterView parent) {
    }

    public void onItemSelected(AdapterView parent, View view, int position, long id) {
    }
	
	public void onClick(View v) {
    }

    private void onLayout() {        
        int positiveButtonResId = 0;
        int negativeButtonResId = R.string.network3g_cancel;
        int neutralButtonResId = 0;
        //super.setTitle(ssid);
        //setInverseBackgroundForced(true);
        boolean defaultPasswordVisibility = true;   
		
        if (mMode == MODE_CONFIGURE) {
            setLayout(R.layout.network3g_con_configure);

			positiveButtonResId = R.string.network3g_connect;  // Connect
			mSaveButtonPos = POSITIVE_BUTTON;
        } else if (mMode == MODE_INFO) {
        	if (mState.isConnectable()) {	            
	            setLayout(R.layout.network3g_con_configure);
        	} else {
				setLayout(R.layout.network3g_con_info);		
			}
			
            if (mState.isConnectable()) {
                super.setTitle(mState.name);
                positiveButtonResId = R.string.network3g_connect;
                mConnectButtonPos = POSITIVE_BUTTON;
            }else{
                positiveButtonResId = R.string.network3g_disconnect;
                mConnectButtonPos = POSITIVE_BUTTON;
            }

            if (mState.isForgetable()) {
                if (positiveButtonResId == 0) {
                    positiveButtonResId = R.string.network3g_forget;
                    mForgetButtonPos = POSITIVE_BUTTON;
                } else {
                    neutralButtonResId = R.string.network3g_forget;
                    mForgetButtonPos = NEUTRAL_BUTTON;
                }
            }
        } 

        setButtons(positiveButtonResId, negativeButtonResId, neutralButtonResId);
    }
	
    private void setButtons(int positiveResId, int negativeResId, int neutralResId) {        
        if (positiveResId > 0) {
            setButton(mContext.getString(positiveResId), this);
        }
        
        if (negativeResId > 0) {
            setButton2(mContext.getString(negativeResId), this);
        }

        if (neutralResId > 0) {
            setButton3(mContext.getString(neutralResId), this);
        }
    }
	
    private void setLayout(int layoutResId) {
        setView(mView = getLayoutInflater().inflate(layoutResId, null));		
        onReferenceViews(mView);
    }	

	private ModemInfo getRow(XmlPullParser parser) {
        if (!"apn".equals(parser.getName())) {
            return null;
        }
		ModemInfo info = new ModemInfo();
        info.modemName  = parser.getAttributeValue(null, "modem");
		info.apn		= parser.getAttributeValue(null, "apn");
        info.user		= parser.getAttributeValue(null, "user");
		info.passwd		= parser.getAttributeValue(null, "password");
		info.dialNum	= parser.getAttributeValue(null, "number");
		info.index		= parser.getAttributeValue(null, "index");
		info.connectName= parser.getAttributeValue(null, "connectName");
		return info;
    }
	
	private void loadApns(XmlPullParser parser) {
		if (parser != null) {
			try {
				mModemList = new ArrayList<ModemInfo>();
				while (true) {
					XmlUtils.nextElement(parser);					
					ModemInfo row = getRow(parser);
					if (row != null) {
						//insertAddingDefaults(db, CARRIERS_TABLE, row);
						mModemList.add(row);
					} else {
						break;	// do we really want to skip the rest of the file?
					}
				}
			} catch (XmlPullParserException e)	{
				Log.e(TAG, "Got execption while getting perferred time zone.", e);
			} catch (IOException e) {
				Log.e(TAG, "Got execption while getting perferred time zone.", e);
			}
		}
	}

	private void dumpApns() {
		int i;
		ModemInfo info;
		for (i=0; i<mModemList.size(); i++) {
			info = mModemList.get(i);
			Log.e(TAG, "The "+i+"th apn information is modem:"+info.modemName+
						" apn:"+info.apn+" num:"+info.dialNum+
						" user:"+info.user+" password:"+info.passwd);
		}
	}

    private void onReferenceViews(View view) {

		// Read external APNS data (partner-provided)
        XmlPullParser confparser = null;
        // Environment.getRootDirectory() is a fancy way of saying ANDROID_ROOT or "/system".
        File confFile = new File(Environment.getRootDirectory(), PARTNER_APNS_PATH);
        FileReader confreader = null;

		try {
            confreader = new FileReader(confFile);
            confparser = Xml.newPullParser();
            confparser.setInput(confreader);
            XmlUtils.beginDocument(confparser, "modem-apns");

            // Sanity check. Force internal version and confidential versions to agree
            int confversion = Integer.parseInt(confparser.getAttributeValue(null, "version"));
			/*
            if (publicversion != confversion) {
                throw new IllegalStateException("Internal APNS file version doesn't match "
                        + confFile.getAbsolutePath());
            }
			*/
            loadApns(confparser);
			mModemName = new String[mModemList.size()];

			dumpApns();
			
			int i;
			ModemInfo info;
			for (i=0; i<mModemList.size(); i++) {
				info = mModemList.get(i);
				mModemName[i] = info.modemName;
			}
			
        } catch (FileNotFoundException e) {
            // It's ok if the file isn't found. It means there isn't a confidential file
            // Log.e(TAG, "File not found: '" + confFile.getAbsolutePath() + "'");
        } catch (Exception e) {
            Log.e(TAG, "Exception while parsing '" + confFile.getAbsolutePath() + "'", e);
        } finally {
            try { if (confreader != null) confreader.close(); } catch (IOException e) { }
        }
		
		if (mMode == MODE_INFO && !mState.isConnectable()) {
            mTable = (ViewGroup) view.findViewById(R.id.table);
			
        } else 
		
		if (mState.isConnectable()) {//(mMode == MODE_CONFIGURE) {
			mNameEdit = (EditText) view.findViewById(R.id.name_edit);
			mNumberEdit = (EditText) view.findViewById(R.id.number_edit);
			mUserEdit = (EditText) view.findViewById(R.id.user_edit);
			mPasswdEdit = (EditText) view.findViewById(R.id.passwd_edit);
			mAPNEdit = (EditText) view.findViewById(R.id.apn_edit); 	
			
			mPinCode = (EditText) view.findViewById(R.id.pin_code_edit);
			mPinCode.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);

			mDeviceSpinner = (Spinner) view.findViewById(R.id.network_devices_spinner);

			mUserEdit.setEnabled(false);
			mPasswdEdit.setEnabled(false);
	
			mdefault_user_pw = (CheckBox) view.findViewById(R.id.default_user_pw);			
			mdefault_user_pw.setChecked(true);
			mdefault_user_pw.setOnClickListener(new View.OnClickListener() {
				public void onClick(View v) {
					if (((CheckBox)v).isChecked()) {
						mUserEdit.setEnabled(false);
						mPasswdEdit.setEnabled(false);			 
					} else {
						mUserEdit.setEnabled(true);
						mPasswdEdit.setEnabled(true);
					}
				}
			});

			mNameEdit.setText(
						mContext.getResources().getString(R.string.network3g_default_connect_name) +
						getConfiguredNum());
			mdefault_user_pw.setChecked(true);

			adapter = new ArrayAdapter<String> (mContext, 
													android.R.layout.simple_spinner_item, 
													mModemName);
			adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
			mDeviceSpinner.setAdapter(adapter);		

			if (mMode == MODE_INFO) {
				mDeviceSpinner.setSelection(getModemOrderFromIndex(Integer.parseInt(mState.device.substring(7))));
				mNameEdit.setText(mState.name);
				mNumberEdit.setText(mState.number);
				mAPNEdit.setText(mState.apn);
				mPinCode.setText(mState.pin_code);
				mPasswdEdit.setText(((mState.passwd).equals("null"))?null:mState.passwd);
				mUserEdit.setText(((mState.user).equals("null"))?null:mState.user);
				mJust_load = true;
			}

			mDeviceSpinner.setOnItemSelectedListener( new AdapterView.OnItemSelectedListener(){
				public void onItemSelected(AdapterView arg0, View arg1, int arg2, long arg3) {
					int device = arg2;
					if (!mJust_load) {
						if (mMode == MODE_INFO) {
							//
						} else {
							if (mModemList.get(device).connectName == null) {
								mNameEdit.setText(
									mContext.getResources().getString(R.string.network3g_default_connect_name) +
									getConfiguredNum());
							} else {
								mNameEdit.setText(mModemList.get(device).connectName);
							}
						}
						mNumberEdit.setText(mModemList.get(device).dialNum);
						mAPNEdit.setText(mModemList.get(device).apn);
						mUserEdit.setText(mModemList.get(device).user);
						mPasswdEdit.setText(mModemList.get(device).passwd);
						mIndex = mModemList.get(device).index;						
					} else {
						mIndex = mModemList.get(device).index;
						mJust_load = false;
					}
				};
				
				public void onNothingSelected(AdapterView<?> arg0) {}
			});	

			
		}
	}
	
    private void onFill() {
        // Appears in the order added
        if (mMode == MODE_INFO && !mState.isConnectable()) {			
            if (mState.configured == Network3gConState.config_connected) {
				addInfoRow(R.string.network3g_apn, mState.apn);
				addInfoRow(R.string.network3g_number, mState.number);				
                addInfoRow(R.string.network3g_ip, 
							SystemProperties.get("net.pppd.ipaddress", ""));
				addInfoRow(R.string.network3g_gw, 
							SystemProperties.get("net.pppd.gateway", ""));
				addInfoRow(R.string.network3g_mask, 
							SystemProperties.get("net.pppd.netmask", ""));
            }		
        }
    }
	
    private void addInfoRow(int nameResId, String value) {
        View rowView = getLayoutInflater().inflate(R.layout.network3g_con_info_row, 
														mTable, false);
        ((TextView) rowView.findViewById(R.id.name)).setText(nameResId);
        ((TextView) rowView.findViewById(R.id.value)).setText(value);
        mTable.addView(rowView);	
    }
	
    private void handleSave() {
        String name = mNameEdit.getText().toString();
        String number = mNumberEdit.getText().toString();
        String user = mUserEdit.getText().toString();
        String passwd = mPasswdEdit.getText().toString();
        String apn = mAPNEdit.getText().toString();
		String pin_code = mPinCode.getText().toString();
		
        int device = mDeviceSpinner.getSelectedItemPosition();

		if(name.length() > 0) mState.setName(name);
		else mState.setName("null");		
		if(number.length() > 0) mState.setNumber(number);
		else mState.setNumber("null");		
		if(user.length() > 0) mState.setUser(user);
		else mState.setUser("null");		
		if(passwd.length() > 0) mState.setPasswd(passwd);
		else mState.setPasswd("null");		
		if(apn.length() > 0) mState.setAPN(apn);
		else mState.setAPN("null");

		if(pin_code.length() > 0) {
			mState.setPinCode(pin_code);
			Log.e(TAG, "Bullshit!!!!!!!!!!!!!!!!!!! 1"+mState.pin_code);
		} else { 
			mState.setPinCode(null);
			Log.e(TAG, "Bullshit!!!!!!!!!!!!!!!!!!! 2"+pin_code);
		}
		
		//mState.setDevice("serial_"+Integer.toString(device));
		mState.setDevice("serial_"+mIndex);

		if (mMode == MODE_INFO) {
			mNetwork3gLayer.forgetNetwork(mState);
		}
				
		int index = Integer.parseInt(getConfiguredNum());
		mState.setIndex(Integer.toString(--index));
		mNetwork3gLayer.Network3gConSetChanged(mState, true);
		
		//mNetwork3gLayer.connectToNetwork(mState, true);
		mNetwork3gLayer.connectToNetwork(mState, mState.isConnectable());
		
        if (!mNetwork3gLayer.saveNetwork(mState)) {
            return;
        }
    }
	
    private void handleForget() {
        //if (!replaceStateWithWifiLayerInstance()) return;
        mNetwork3gLayer.connectToNetwork(mState, false);
        mNetwork3gLayer.forgetNetwork(mState);
    }
    
    private void handleConnect() {
        mNetwork3gLayer.connectToNetwork(mState, mState.isConnectable());
    }
	
    public void setState(Network3gConState state) {
        mState = state;
    }
	
    public void setMode(int mode) {
        mMode = mode;
    }

	private String getConfiguredNum () {
		SharedPreferences store_config_num;
		store_config_num = mContext.getSharedPreferences("num", 0);
		String numS = store_config_num.getString("num", "");
		int numI;
		
		if( numS.length() > 0 ){
			numI = Integer.parseInt(numS) + 1;				
		} else {
			numI = 1;
		}

		return Integer.toString(numI);  
	}
}


