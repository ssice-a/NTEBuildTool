class UBPUI_CheckBox_switch_C : public UHTUI_CheckBox
{
public:
    class UWidgetTree* WidgetTree = "WidgetTree'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:WidgetTree'";
    bool bClassRequiresNativeTick = true;
    TArray<class UWidgetAnimation*> Animations = {
        "WidgetAnimation'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:HoveredAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:SelectAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:UnselectAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:ReceiveNavigationAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:LostNavigationAnim_INST'"
    };
    TArray<FName> NamedSlots = {
        FName("NamedSlot_52")
    };
    TArray<FName> AvailableNamedSlots = {
        FName("NamedSlot_52")
    };
    TArray<FName> InstanceNamedSlots = {
        FName("NamedSlot_52")
    };
    class UFunction* UberGraphFunction = "Function'/Game/UI/Blueprints/BPUI_CheckBox_switch.BPUI_CheckBox_switch_C:ExecuteUbergraph_BPUI_CheckBox_switch'";
    struct FPointerToUberGraphFrame UberGraphFrame = {};
    struct FSlateBrush CheckedSlate = {
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(108, 56),
        "ResourceObject": "Texture2D'/Game/UI/UI/selfie/YH_UI_Photo_toggle01.YH_UI_Photo_toggle01'"
    };
    struct FSlateBrush UnCheckedSlate = {
        "ImageType": ESlateBrushImageType::FullColor,
        "ImageSize": FVector2D(108, 56),
        "ResourceObject": "Texture2D'/Game/UI/UI/selfie/YH_UI_Photo_toggle.YH_UI_Photo_toggle'"
    };
    FSoftObjectPath CheckSound = FSoftObjectPath("/Game/WwiseAudio/Events/UI/UI/Play_Sfx_UI_Set_PullDown_Menu.Play_Sfx_UI_Set_PullDown_Menu");
    FSoftObjectPath UncheckSound = FSoftObjectPath("/Game/WwiseAudio/Events/UI/UI/Play_Sfx_UI_Set_PullDown_Menu_Select.Play_Sfx_UI_Set_PullDown_Menu_Select");
    bool bHasScriptImplementedTick = false;
    bool bHasScriptImplementedPaint = false;
    class UNamedSlot* NamedSlot_52;
    class UImage* ImgCheck_Naviable_Fx;
    class UImage* ImgCheck_Naviable_Arrow;
    class UImage* ImgCheck_Naviable;
    class UImage* Image_Select;
    class UImage* Image_Hover;

    // (Final, UbergraphFunction)
    private void ExecuteUbergraph_BPUI_CheckBox_switch(int EntryPoint)
    {
        goto EntryPoint; // EX_ComputedJump
    
        Label_10:
        return; // EX_Return
    }

    // (BlueprintCosmetic, Event, Public, BlueprintEvent)
    public void Construct()
    {
        ExecuteUbergraph_BPUI_CheckBox_switch(10); // EX_LocalFinalFunction
    
        return; // EX_Return
    }
};