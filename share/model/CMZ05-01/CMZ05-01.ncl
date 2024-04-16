items:
  id: 0
  name: "Root"
  plugin: Base
  class: RootItem
  children:
    -
      id: 1
      name: "CMZ05-01"
      plugin: Body
      class: BodyItem
      is_checked: true
      data:
        file: "CMZ05-01/CMZ05-01.bodyb"
        format: CHOREONOID-BODY
        rootPosition: [ 0, 0, 0 ]
        rootAttitude: [
          1, 0, 0,
          0, 1, 0,
          0, 0, 1 ]
        jointDisplacements: [ 0, 90, 0, 0, 0, 0 ]
        jointPositions: [ 0, 1.5708, 0, 0, 0, 0 ]
        initialRootPosition: [ 0, 0, 0 ]
        initialRootAttitude: [
          1, 0, 0,
          0, 1, 0,
          0, 0, 1 ]
        initialJointPositions: [ 0, 1.5708, 0, 0, 0, 0 ]
        staticModel: false
        collisionDetection: false
        selfCollisionDetection: false
        location_editable: true
        scene_sensitive: true
        link_kinematics:
          FLANGE:
            base_frame: 0
            offset_frame: 0
        enable_attachment: true
        zmp: [ 0, 0, 0 ]
      addons:
        -
          name: CellItem
          plugin: NachiExtension
          cell_item_type: robot
      children:
        -
          id: 2
          name: "CMZ05"
          plugin: NachiExtension
          class: NachiControllerItem
          data:
            speed_ratio: 1
          children:
            -
              id: 3
              name: "Program"
              plugin: Base
              class: FolderItem
            -
              id: 4
              name: "Tool"
              plugin: Body
              class: LinkOffsetFrameListItem
              data:
                itemization: independent
                type: CoordinateFrameList
                format_version: 1
                frame_type: offset
            -
              id: 5
              name: "Base Coordinate"
              plugin: Base
              class: CoordinateFrameListItem
              data:
                itemization: sub
                type: CoordinateFrameList
                format_version: 1
                frame_type: base
            -
              id: 6
              name: "Global Variable"
              plugin: Manipulator
              class: MprMultiVariableListItem
              data:
                variable_lists:
                  -
                    type: ManipulatorVariableList
                    format_version: 1
                    variable_type: integer
                    is_number_id_enabled: true
                  -
                    type: ManipulatorVariableList
                    format_version: 1
                    variable_type: real
                    is_number_id_enabled: true
                  -
                    type: ManipulatorVariableList
                    format_version: 1
                    variable_type: string
                    is_number_id_enabled: true
        -
          id: 7
          name: "DigitalIoDevice"
          plugin: Body
          class: DeviceOverwriteItem
          is_attached_item: true
          data:
            num_signal_lines: 2049
            mediator: nachi_cell_item
            device_type: DigitalIoDevice
            link_name: "BASE"
        -
          id: 8
          name: "CMZ05ex"
          plugin: NachiExtension
          class: NachiExternalControllerItem
