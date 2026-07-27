class UUI_Common_Popup_C : public UHTUI_PopupWindow_Base
{
public:
    class UWidgetTree* WidgetTree = "WidgetTree'/Game/UI/Blueprints/UI_Common_Popup.UI_Common_Popup_C:WidgetTree'";
    bool bClassRequiresNativeTick = true;
    TArray<class UWidgetAnimation*> Animations = {
        "WidgetAnimation'/Game/UI/Blueprints/UI_Common_Popup.UI_Common_Popup_C:OpenAnim_INST'",
        "WidgetAnimation'/Game/UI/Blueprints/UI_Common_Popup.UI_Common_Popup_C:CloseAnim_INST'"
    };
    class UFunction* UberGraphFunction = "Function'/Game/UI/Blueprints/UI_Common_Popup.UI_Common_Popup_C:ExecuteUbergraph_UI_Common_Popup'";
    TMap<FName, struct FGuid> CookedPropertyGuids = {
        { FName("Use Close Button"), FGuid(0xB9A8280D, 0x4F8D32B9, 0x34F667A4, 0x2BE1BD2D) }, 
        { FName("PopupTitle Visible"), FGuid(0x54701685, 0x4D4249F6, 0x78B0A388, 0x1FD453A4) }, 
        { FName("PopupTitle"), FGuid(0x17292824, 0x4EC42D62, 0x295FDB8A, 0x62CF335B) }, 
        { FName("Image"), FGuid(0xD570E07B, 0x4FE726E0, 0xFB6BB9AD, 0x5469CC05) }, 
        { FName("Image_84"), FGuid(0x6192AC75, 0x46FCF4D4, 0xF83AF083, 0x04E82FFF) }, 
        { FName("Image_Bg"), FGuid(0x302C8DE3, 0x4C11CB1F, 0x67B49296, 0x2A5E2542) }, 
        { FName("Image_Bg_95"), FGuid(0x63EF4735, 0x1BBA3F7F, 0x801E24A1, 0xE9417F15) }, 
        { FName("Image_Bg_OutLine"), FGuid(0x8893FD6D, 0x4C878D93, 0xBF2C008F, 0x66039BC8) }, 
        { FName("Image_Glow_01"), FGuid(0x2FB15F55, 0x4CB1C25B, 0xAC89E1B7, 0x43BA97BF) }, 
        { FName("Image_Glow_02"), FGuid(0xF36BDBD9, 0x4331BCCA, 0xFD70E892, 0x0544747F) }, 
        { FName("TextBlock"), FGuid(0xD68855BD, 0x46ED71EF, 0x5FECBE9F, 0xE10522BF) }
    };
    struct FPointerToUberGraphFrame UberGraphFrame = {};
    FText PopupTitle = FText("弹窗标题", CultureInvariant);
    bool PopupTitle Visible = true;
    bool Use Close Button = true;
    bool bHasScriptImplementedTick = false;
    bool bHasScriptImplementedPaint = false;
    class UTextBlock* TextBlock;
    class UImage* Image_Glow_02;
    class UImage* Image_Glow_01;
    class UImage* Image_Bg_OutLine;
    class UImage* Image_Bg_95;
    class UImage* Image_Bg;
    class UImage* Image_84;
    class UImage* Image;

    // (Final, UbergraphFunction)
    private void ExecuteUbergraph_UI_Common_Popup(int EntryPoint)
    {
        goto EntryPoint; // EX_ComputedJump
    
        Label_10:
        TextBlock->SetText(PopupTitle); // EX_Context
    
        Temp_byte_Variable = 0x3; // EX_Let
    
        Temp_byte_Variable_1 = 0x1; // EX_Let
    
        Temp_bool_Variable = PopupTitle Visible; // EX_LetBool
    
        TextBlock->SetVisibility(Temp_bool_Variable ? Temp_byte_Variable : Temp_byte_Variable_1); // EX_Context
    
        Temp_bool_Variable_1 = Use Close Button; // EX_LetBool
    
        Temp_byte_Variable_2 = 0x0; // EX_Let
    
        Temp_byte_Variable_3 = 0x1; // EX_Let
    
        CBActionBar_Close->SetVisibility(Temp_bool_Variable_1 ? Temp_byte_Variable_2 : Temp_byte_Variable_3); // EX_Context
    
        return; // EX_Jump
    
        Label_356:
        return; // EX_Return
    }

    // (BlueprintCosmetic, Event, Public, BlueprintEvent)
    public void Construct()
    {
        ExecuteUbergraph_UI_Common_Popup(356); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (BlueprintCosmetic, Event, Public, BlueprintEvent)
    public void PreConstruct(bool IsDesignTime)
    {
        UberGraphFrame->K2Node_Event_IsDesignTime = IsDesignTime; // EX_LetValueOnPersistentFrame
    
        ExecuteUbergraph_UI_Common_Popup(10); // EX_LocalFinalFunction
    
        return; // EX_Return
    }

    // (Public, HasDefaults, BlueprintCallable, BlueprintEvent)
    public void BP_CloseAnim()
    {
        CallFunc_PlayAnimation_ReturnValue = UUserWidget::PlayAnimation(CloseAnim, 0, 1, 0x0, 1, false); // EX_Let
    
        return; // EX_Return
    }
};