package com.btspeaker.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.enableEdgeToEdge
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import com.btspeaker.app.ui.MainPanel

class MainActivity : ComponentActivity() {
    private val vm: SpeakerViewModel by viewModels()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { MainPanel(vm) }
    }
}